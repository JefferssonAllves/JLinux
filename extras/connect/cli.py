#!/usr/bin/env python3
"""Local pairing and private Tailscale publication for JLinux Connect."""
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys

from server import STATE, pairing_key


def service(*arguments):
    runtime = f'/run/user/{os.getuid()}'
    env = dict(os.environ, XDG_RUNTIME_DIR=runtime, DBUS_SESSION_BUS_ADDRESS=f'unix:path={runtime}/bus')
    return subprocess.run(['systemctl', '--user', *arguments, 'jlinux-connect.service'], env=env, check=True)


def tailnet():
    result = subprocess.run(['tailscale', 'status', '--json'], capture_output=True, text=True, timeout=10)
    try:
        status = json.loads(result.stdout)
    except ValueError:
        return None
    if status.get('BackendState') != 'Running':
        return None
    return status.get('Self', {}).get('DNSName', '').rstrip('.') or None


def main():
    if os.getuid() == 0:
        raise SystemExit('Execute com seu usuário do JLinux, sem sudo.')
    command = sys.argv[1] if len(sys.argv) > 1 else 'status'
    if command == 'conectar':
        subprocess.run(['sudo', 'systemctl', 'enable', '--now', 'tailscaled'], check=True)
        if not tailnet():
            subprocess.run(['sudo', 'tailscale', 'up', '--timeout=5m'], check=True)
        host = tailnet()
        if not host:
            raise SystemExit('Conclua o login no Tailscale e tente novamente.')
        current = subprocess.run(['tailscale', 'serve', 'status', '--json'], capture_output=True, text=True, check=True)
        config = json.loads(current.stdout or '{}')
        if config.get('TCP', {}).get('443') and 'http://127.0.0.1:8765' not in current.stdout:
            raise SystemExit('A porta HTTPS 443 já pertence a outro serviço do Tailscale; configuração preservada.')
        subprocess.run(['sudo', 'tailscale', 'serve', '--bg', '--https=443',
                        'http://127.0.0.1:8765'], check=True)
        pairing_key(STATE)
        (STATE / 'host').write_text(host)
        service('restart')
        print('Acesso privado configurado. No iPhone, entre na mesma conta do Tailscale.')
        print('Execute jlinux-connect parear para obter o link de conexão.')
    elif command == 'parear':
        host = tailnet()
        if host and (STATE / 'host').exists():
            url = f'https://{host}/'
        else:
            print('Para conectar o iPhone, primeiro execute: jlinux-connect conectar')
            print('O endereço abaixo funciona apenas neste computador.')
            url = 'http://127.0.0.1:8765/'
        link = url + '#' + pairing_key(STATE)
        print('\nLink privado de conexão (não compartilhe):\n' + link)
        if shutil.which('qrencode'):
            print('\nEscaneie com a câmera do iPhone:')
            subprocess.run(['qrencode', '-t', 'UTF8', '-m', '2'], input=link, text=True, check=True)
        print('\nNo Safari do iPhone: Compartilhar → Adicionar à Tela de Início.')
    elif command == 'revogar':
        pairing_key(STATE)
        (STATE / 'pair.key').write_text(secrets.token_urlsafe(32))
        (STATE / 'sessions.json').unlink(missing_ok=True)
        service('restart')
        print('Todas as sessões foram encerradas e a chave anterior foi revogada.')
    elif command == 'iniciar':
        service('start')
    elif command == 'parar':
        service('stop')
        print('Acesso ao terminal e à tela interrompido.')
    elif command == 'status':
        service('--no-pager', 'status')
        host = tailnet()
        print('\nTailscale: ' + (host or 'aguardando vinculação da conta'))
        print('Vincular: jlinux-connect conectar | Link: jlinux-connect parear')
        print('Parar acesso: jlinux-connect parar | Revogar dispositivos: jlinux-connect revogar')
    else:
        raise SystemExit('Uso: jlinux-connect [status|conectar|parear|iniciar|parar|revogar]')


if __name__ == '__main__':
    try:
        main()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f'Não foi possível concluir a configuração ({error}).')
