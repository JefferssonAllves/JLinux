#define main files_application_main
#include "../src/files/main.c"
#undef main
#include <glib/gstdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static bool finish(struct app *app){
    if(!app->operation){return false;}
    int status;waitpid(app->operation,&status,0);if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){char msg[512]={0};lseek(app->operation_log,0,SEEK_SET);ssize_t n=read(app->operation_log,msg,511);(void)n;fprintf(stderr,"%s",msg);}close(app->operation_log);app->operation=0;
    return WIFEXITED(status)&&WEXITSTATUS(status)==0;
}
int main(void){
    CHECK(!g_mkdir_with_parents(g_get_user_cache_dir(),0700));char *root=g_build_filename(g_get_user_cache_dir(),"jlinux-actions-XXXXXX",NULL);CHECK(g_mkdtemp(root));
    char *data=g_build_filename(root,"data",NULL);g_setenv("XDG_DATA_HOME",data,TRUE);
    struct app app={0};CHECK(navigate(&app,root));
    dialog_open(&app,1);g_strlcpy(app.input,"Folder á",sizeof(app.input));dialog_accept(&app);CHECK(finish(&app));
    char *folder=g_build_filename(root,"Folder á",NULL);CHECK(g_file_test(folder,G_FILE_TEST_IS_DIR));
    dialog_open(&app,2);g_strlcpy(app.input,"file $(touch SENTINEL).txt",sizeof(app.input));dialog_accept(&app);
    char *file=g_build_filename(root,"file $(touch SENTINEL).txt",NULL);CHECK(g_file_test(file,G_FILE_TEST_EXISTS));
    CHECK(g_file_set_contents(file,"preserve",-1,NULL));
    dialog_open(&app,2);g_strlcpy(app.input,"file $(touch SENTINEL).txt",sizeof(app.input));dialog_accept(&app);
    char *contents=NULL;CHECK(g_file_get_contents(file,&contents,NULL,NULL));CHECK(!strcmp(contents,"preserve"));g_free(contents);
    CHECK(navigate(&app,root));app.selected=1;menu_action(&app,1);CHECK(app.clipboard);
    CHECK(navigate(&app,folder));menu_action(&app,3);CHECK(finish(&app));
    char *copy=g_build_filename(folder,"file $(touch SENTINEL).txt",NULL);CHECK(g_file_test(copy,G_FILE_TEST_EXISTS));
    menu_action(&app,3);CHECK(!app.operation);CHECK(strstr(app.message,"destino"));
    CHECK(navigate(&app,folder));app.selected=0;dialog_open(&app,3);g_strlcpy(app.input,"renamed.txt",sizeof(app.input));dialog_accept(&app);CHECK(finish(&app));
    char *renamed=g_build_filename(folder,"renamed.txt",NULL);CHECK(g_file_test(renamed,G_FILE_TEST_EXISTS));CHECK(!g_file_test(copy,G_FILE_TEST_EXISTS));
    CHECK(navigate(&app,folder));app.selected=0;menu_action(&app,2);CHECK(navigate(&app,root));menu_action(&app,3);CHECK(finish(&app));
    char *moved=g_build_filename(root,"renamed.txt",NULL);CHECK(g_file_test(moved,G_FILE_TEST_EXISTS));CHECK(!g_file_test(renamed,G_FILE_TEST_EXISTS));
    CHECK(navigate(&app,root));app.selected=2;dialog_open(&app,5);dialog_accept(&app);CHECK(finish(&app));CHECK(!g_file_test(moved,G_FILE_TEST_EXISTS));
    char *extra=g_build_filename(root,"second.txt",NULL);CHECK(g_file_set_contents(extra,"two",-1,NULL));
    CHECK(navigate(&app,root));app.selected=1;
    app.selection=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
    g_hash_table_add(app.selection,g_strdup("file $(touch SENTINEL).txt"));g_hash_table_add(app.selection,g_strdup("second.txt"));
    menu_action(&app,1);CHECK(app.clipboard_paths->len==2);CHECK(navigate(&app,folder));menu_action(&app,3);CHECK(finish(&app));
    char *second_copy=g_build_filename(folder,"second.txt",NULL);CHECK(g_file_test(second_copy,G_FILE_TEST_EXISTS));CHECK(g_file_test(copy,G_FILE_TEST_EXISTS));g_free(second_copy);g_free(extra);
    char *sentinel=g_build_filename(root,"SENTINEL",NULL);CHECK(!g_file_test(sentinel,G_FILE_TEST_EXISTS));
    g_unlink(file);g_rmdir(folder);g_free(app.clipboard);g_free(app.dialog_path);if(app.selection){g_hash_table_destroy(app.selection);}if(app.clipboard_paths){g_ptr_array_unref(app.clipboard_paths);}if(app.dialog_paths){g_ptr_array_unref(app.dialog_paths);}browser_finish(&app.browser);
    /* The test owns this private directory, including its isolated trash. */
    char *args[]={"rm","-r","--",root,NULL};g_spawn_sync(NULL,args,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,NULL,NULL,NULL,NULL);
    g_free(sentinel);g_free(moved);g_free(renamed);g_free(copy);g_free(file);g_free(folder);g_free(data);g_free(root);
    puts("File creation, collision protection, copy, rename, cut, trash and literal filenames passed.");return 0;
}
