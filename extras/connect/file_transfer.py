"""Stream home-directory transfers through directory FDs, never through symlinks."""
import asyncio
import contextlib
import errno
import os
from pathlib import Path, PurePosixPath
import secrets
import stat
from urllib.parse import quote

from aiohttp import web

MAX_UPLOAD = 2 * 1024**3
CHUNK = 256 * 1024


def parts(value):
    if not isinstance(value, str) or len(value) > 2048 or '\0' in value:
        raise web.HTTPBadRequest(text='Caminho inválido')
    try:
        value.encode('utf-8')
    except UnicodeError:
        raise web.HTTPBadRequest(text='Nome de arquivo inválido')
    path = PurePosixPath(value)
    if path.is_absolute() or '..' in path.parts:
        raise web.HTTPForbidden(text='Escolha uma pasta dentro dos seus arquivos pessoais')
    return path.parts


def file_error(error):
    if error.errno == errno.EEXIST:
        return web.HTTPConflict(text='Já existe um arquivo com esse nome. Renomeie antes de enviar.')
    if error.errno in (errno.ELOOP, errno.EACCES, errno.EPERM):
        return web.HTTPForbidden(text='Arquivo ou pasta sem permissão, ou link simbólico não permitido')
    if error.errno in (errno.ENOENT, errno.ENOTDIR):
        return web.HTTPNotFound(text='Arquivo ou pasta não encontrado')
    if error.errno in (errno.ENOSPC, errno.EDQUOT):
        return web.HTTPInsufficientStorage(text='Não há espaço disponível para concluir o envio')
    return web.HTTPBadRequest(text='Não foi possível acessar esse arquivo')


async def io(function, *args):
    # A cancelled request must not close/reuse an FD while its worker still uses it.
    task = asyncio.create_task(asyncio.to_thread(function, *args))
    try:
        return await asyncio.shield(task)
    except asyncio.CancelledError:
        with contextlib.suppress(OSError):
            await task
        raise


class FileTransfer:
    def __init__(self, root):
        self.root = Path(root).resolve()
        self.upload_lock = asyncio.Lock()

    def directory(self, path):
        fd = os.open(self.root, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | os.O_NOFOLLOW)
        try:
            for part in parts(path):
                new = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | os.O_NOFOLLOW, dir_fd=fd)
                os.close(fd)
                fd = new
            return fd
        except BaseException:
            os.close(fd)
            raise

    async def listing(self, request):
        path = request.query.get('path', '')
        try:
            fd = self.directory(path)
            entries = []
            truncated = False
            try:
                with os.scandir(fd) as files:
                    for item in files:
                        if item.name.startswith('.jlinux-upload-') or (item.name.startswith('.') and request.query.get('hidden') != '1'):
                            continue
                        try:
                            item.name.encode('utf-8')
                            info = item.stat(follow_symlinks=False)
                        except (OSError, UnicodeError):
                            continue
                        if not stat.S_ISREG(info.st_mode) and not stat.S_ISDIR(info.st_mode):
                            continue
                        if len(entries) >= 1000:
                            truncated = True
                            break
                        entries.append({'name': item.name, 'directory': stat.S_ISDIR(info.st_mode),
                                        'size': info.st_size, 'modified': info.st_mtime})
            finally:
                os.close(fd)
            entries.sort(key=lambda item: (not item['directory'], item['name'].casefold()))
            return web.json_response({'path': '/'.join(parts(path)), 'entries': entries, 'truncated': truncated,
                                      'max_upload': MAX_UPLOAD})
        except OSError as error:
            raise file_error(error)

    async def download(self, request):
        path = parts(request.query.get('path', ''))
        if not path:
            raise web.HTTPBadRequest(text='Escolha um arquivo para baixar')
        directory = fd = None
        try:
            directory = self.directory('/'.join(path[:-1]))
            fd = os.open(path[-1], os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC | os.O_NOFOLLOW, dir_fd=directory)
            info = os.fstat(fd)
            if not stat.S_ISREG(info.st_mode):
                raise web.HTTPForbidden(text='Apenas arquivos comuns podem ser baixados')
            response = web.StreamResponse(headers={'Content-Type': 'application/octet-stream',
                'Content-Length': str(info.st_size),
                'Content-Disposition': "attachment; filename*=UTF-8''" + quote(path[-1], safe='')})
            await response.prepare(request)
            remaining = info.st_size
            while remaining:
                data = await io(os.read, fd, min(CHUNK, remaining))
                if not data:
                    break
                remaining -= len(data)
                await response.write(data)
            return response
        except OSError as error:
            raise file_error(error)
        finally:
            if fd is not None:
                os.close(fd)
            if directory is not None:
                os.close(directory)

    async def upload(self, request):
        name = request.query.get('name', '')
        if not name or len(parts(name)) != 1 or '/' in name or name in ('.', '..') or len(name.encode()) > 255:
            raise web.HTTPBadRequest(text='Nome de arquivo inválido')
        if request.content_length is not None and request.content_length > MAX_UPLOAD:
            raise web.HTTPRequestEntityTooLarge(max_size=MAX_UPLOAD, actual_size=request.content_length)
        if self.upload_lock.locked():
            raise web.HTTPConflict(text='Um envio já está em andamento. Aguarde para enviar outro arquivo.')
        async with self.upload_lock:
            directory = fd = None
            temporary = '.jlinux-upload-' + secrets.token_hex(12)
            try:
                directory = self.directory(request.query.get('path', ''))
                try:
                    os.stat(name, dir_fd=directory, follow_symlinks=False)
                except FileNotFoundError:
                    pass
                else:
                    raise web.HTTPConflict(text='Já existe um arquivo com esse nome. Renomeie antes de enviar.')
                fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_CLOEXEC | os.O_NOFOLLOW,
                             0o600, dir_fd=directory)
                size = 0
                async for data in request.content.iter_chunked(CHUNK):
                    size += len(data)
                    if size > MAX_UPLOAD:
                        raise web.HTTPRequestEntityTooLarge(max_size=MAX_UPLOAD, actual_size=size)
                    await io(self.write, fd, data)
                await io(os.fsync, fd)
                os.close(fd)
                fd = None
                os.link(temporary, name, src_dir_fd=directory, dst_dir_fd=directory, follow_symlinks=False)
                await io(os.fsync, directory)
                return web.json_response({'ok': True, 'name': name, 'size': size}, status=201)
            except OSError as error:
                raise file_error(error)
            finally:
                if fd is not None:
                    os.close(fd)
                if directory is not None:
                    with contextlib.suppress(FileNotFoundError):
                        os.unlink(temporary, dir_fd=directory)
                    os.close(directory)

    @staticmethod
    def write(fd, data):
        while data:
            count = os.write(fd, data)
            if count <= 0:
                raise OSError(errno.ENOSPC, 'write failed')
            data = data[count:]
