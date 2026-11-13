#include "media.h"
#include <string.h>
#define PLAYER "org.mpris.MediaPlayer2.Player"
static void changed(GDBusConnection *,const char *,const char *,const char *,const char *,GVariant *,gpointer);
static void copy_text(char *dest,gsize size,const char *value){
    g_strlcpy(dest,value,size);const char *end;
    if(!g_utf8_validate(dest,-1,&end)){dest[end-dest]=0;}
}
static void properties(struct media *m, GVariant *props) {
    const char *status;
    if (g_variant_lookup(props, "PlaybackStatus", "&s", &status)) { m->playing = !strcmp(status, "Playing"); }
    gboolean value;
    if(g_variant_lookup(props,"CanControl","b",&value)){m->can_control=value;}
    if (g_variant_lookup(props,"CanPlay","b",&value)) { m->can_play=value; }
    if (g_variant_lookup(props,"CanPause","b",&value)) { m->can_pause=value; }
    if (g_variant_lookup(props,"CanGoPrevious","b",&value)) { m->can_previous=value; }
    if (g_variant_lookup(props,"CanGoNext","b",&value)) { m->can_next=value; }
    GVariant *metadata = g_variant_lookup_value(props,"Metadata",G_VARIANT_TYPE_VARDICT);
    if (metadata) {
        const char *title="";g_variant_lookup(metadata,"xesam:title","&s",&title);
        copy_text(m->title,sizeof(m->title),title);
        GVariant *artists=g_variant_lookup_value(metadata,"xesam:artist",G_VARIANT_TYPE_STRING_ARRAY);
        m->artist[0]=0;
        if(artists){gsize n;const char **names=g_variant_get_strv(artists,&n);if(n){copy_text(m->artist,sizeof(m->artist),names[0]);}g_free(names);g_variant_unref(artists);}
        g_variant_unref(metadata);
    }
    m->dirty=true;
}
struct query {struct media *media;guint generation;};
static void got_properties(GObject *source,GAsyncResult *result,gpointer data){
    struct query *query=data;struct media *m=query->media;m->pending--;GVariant *reply=g_dbus_connection_call_finish(G_DBUS_CONNECTION(source),result,NULL);
    if(reply&&(query->generation!=m->generation||g_cancellable_is_cancelled(m->cancel))){g_variant_unref(reply);reply=NULL;}g_free(query);
    if(reply){GVariant *props;g_variant_get(reply,"(@a{sv})",&props);properties(m,props);g_variant_unref(props);g_variant_unref(reply);}
}
static void read_properties(struct media *m){
    if(m->player){struct query *query=g_new(struct query,1);*query=(struct query){m,m->generation};m->pending++;g_dbus_connection_call(m->bus,m->player,"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","GetAll",g_variant_new("(s)",PLAYER),G_VARIANT_TYPE("(a{sv})"),G_DBUS_CALL_FLAGS_NONE,2000,m->cancel,got_properties,query);}
}
static void names_ready(GObject *source,GAsyncResult *result,gpointer data){
    struct media *m=data;m->pending--;GVariant *reply=g_dbus_connection_call_finish(G_DBUS_CONNECTION(source),result,NULL);
    if(!reply){return;}if(g_cancellable_is_cancelled(m->cancel)){g_variant_unref(reply);return;}GVariant *names=g_variant_get_child_value(reply,0);GVariantIter iter;const char *name;
    g_variant_iter_init(&iter,names);
    if(!m->player){while(g_variant_iter_next(&iter,"&s",&name)){if(g_str_has_prefix(name,"org.mpris.MediaPlayer2.")){m->player=g_strdup(name);m->properties=g_dbus_connection_signal_subscribe(m->bus,m->player,"org.freedesktop.DBus.Properties","PropertiesChanged","/org/mpris/MediaPlayer2",PLAYER,G_DBUS_SIGNAL_FLAGS_NONE,changed,m,NULL);read_properties(m);break;}}}
    g_variant_unref(names);g_variant_unref(reply);m->dirty=true;
}
static void list_players(struct media *m){m->pending++;g_dbus_connection_call(m->bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","ListNames",NULL,G_VARIANT_TYPE("(as)"),G_DBUS_CALL_FLAGS_NONE,2000,m->cancel,names_ready,m);}
static void changed(GDBusConnection *bus,const char *sender,const char *path,const char *interface,const char *signal,GVariant *params,gpointer data){
    (void)bus;(void)sender;(void)path;(void)interface;(void)signal;
    struct media *m=data;const char *iface;GVariant *props,*invalid;
    g_variant_get(params,"(&s@a{sv}@as)",&iface,&props,&invalid);
    /* Only the chosen player's properties may change this panel. */
    if(m->player&&!strcmp(iface,PLAYER)){
        properties(m,props);
        if(g_variant_n_children(invalid)){read_properties(m);}
    }
    g_variant_unref(props);g_variant_unref(invalid);
}
static void owner_changed(GDBusConnection *bus,const char *sender,const char *path,const char *interface,const char *signal,GVariant *params,gpointer data){
    (void)bus;(void)sender;(void)path;(void)interface;(void)signal;
    struct media *m=data;const char *name,*old,*next;g_variant_get(params,"(&s&s&s)",&name,&old,&next);(void)next;
    if(!g_str_has_prefix(name,"org.mpris.MediaPlayer2.")){return;}
    if(m->player&&!strcmp(m->player,name)&&*old){
        if(m->properties){g_dbus_connection_signal_unsubscribe(m->bus,m->properties);m->properties=0;}
        m->generation++;g_clear_pointer(&m->player,g_free);m->title[0]=m->artist[0]=0;m->playing=m->can_play=m->can_pause=m->can_previous=m->can_next=false;m->dirty=true;
    }
    list_players(m);
}
static void connected(GObject *source,GAsyncResult *result,gpointer data){
    (void)source;struct media *m=data;m->pending--;m->bus=g_bus_get_finish(result,NULL);if(!m->bus||g_cancellable_is_cancelled(m->cancel)){return;}
    m->owners=g_dbus_connection_signal_subscribe(m->bus,"org.freedesktop.DBus","org.freedesktop.DBus","NameOwnerChanged","/org/freedesktop/DBus",NULL,G_DBUS_SIGNAL_FLAGS_NONE,owner_changed,m,NULL);
    list_players(m);
}
void media_init(struct media *m){memset(m,0,sizeof(*m));m->can_control=true;m->cancel=g_cancellable_new();m->pending++;g_bus_get(G_BUS_TYPE_SESSION,m->cancel,connected,m);}
void media_action(struct media *m,int action){
    if(!m->bus||!m->player||!m->can_control){return;}
    if((action==0&&!m->can_previous)||(action==1&&!(m->playing?m->can_pause:m->can_play))||(action==2&&!m->can_next)||action<0||action>2){return;}
    const char *methods[]={"Previous","PlayPause","Next"};
    g_dbus_connection_call(m->bus,m->player,"/org/mpris/MediaPlayer2",PLAYER,methods[action],NULL,NULL,G_DBUS_CALL_FLAGS_NONE,2000,m->cancel,NULL,NULL);
}
void media_finish(struct media *m){
    g_cancellable_cancel(m->cancel);
    if(m->bus){if(m->owners){g_dbus_connection_signal_unsubscribe(m->bus,m->owners);}if(m->properties){g_dbus_connection_signal_unsubscribe(m->bus,m->properties);}}
    /* Drain cancelled callbacks before releasing their state. */
    while(m->pending){g_main_context_iteration(NULL,true);}
    g_clear_object(&m->bus);g_clear_object(&m->cancel);g_clear_pointer(&m->player,g_free);
}
