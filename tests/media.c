#include "../src/shell/media.h"
#include <string.h>
static int calls[3];
static const char xml[]="<node><interface name='org.mpris.MediaPlayer2.Player'><method name='Previous'/><method name='PlayPause'/><method name='Next'/><property name='PlaybackStatus' type='s' access='read'/><property name='Metadata' type='a{sv}' access='read'/><property name='CanPlay' type='b' access='read'/><property name='CanPause' type='b' access='read'/><property name='CanGoNext' type='b' access='read'/><property name='CanGoPrevious' type='b' access='read'/></interface></node>";
static void call(GDBusConnection *c,const char *sender,const char *path,const char *iface,const char *method,GVariant *params,GDBusMethodInvocation *inv,gpointer data){
    (void)c;(void)sender;(void)path;(void)iface;(void)params;(void)data;
    if(!strcmp(method,"Previous")){calls[0]++;}else if(!strcmp(method,"PlayPause")){calls[1]++;}else if(!strcmp(method,"Next")){calls[2]++;}
    g_dbus_method_invocation_return_value(inv,NULL);
}
static GVariant *get(GDBusConnection *c,const char *sender,const char *path,const char *iface,const char *prop,GError **error,gpointer data){
    (void)c;(void)sender;(void)path;(void)iface;(void)error;(void)data;
    if(!strcmp(prop,"PlaybackStatus")){return g_variant_new_string("Playing");}
    if(!strcmp(prop,"Metadata")){GVariantBuilder b;g_variant_builder_init(&b,G_VARIANT_TYPE_VARDICT);g_variant_builder_add(&b,"{sv}","xesam:title",g_variant_new_string("Faixa de teste"));const char *artists[]={"Artista",NULL};g_variant_builder_add(&b,"{sv}","xesam:artist",g_variant_new_strv(artists,-1));return g_variant_builder_end(&b);}
    return g_variant_new_boolean(true);
}
static void spin(void){gint64 end=g_get_monotonic_time()+500000;do{while(g_main_context_iteration(NULL,false)){}g_usleep(1000);}while(g_get_monotonic_time()<end);}
int main(void){
    GDBusConnection *bus=g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);g_assert_nonnull(bus);
    GDBusNodeInfo *node=g_dbus_node_info_new_for_xml(xml,NULL);const GDBusInterfaceVTable vtable={.method_call=call,.get_property=get};
    guint object=g_dbus_connection_register_object(bus,"/org/mpris/MediaPlayer2",node->interfaces[0],&vtable,NULL,NULL,NULL);g_assert_cmpuint(object,>,0);
    GVariant *reply=g_dbus_connection_call_sync(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","RequestName",g_variant_new("(su)","org.mpris.MediaPlayer2.JLinuxTest",0),NULL,0,2000,NULL,NULL);g_assert_nonnull(reply);g_variant_unref(reply);
    struct media m;media_init(&m);spin();g_assert_nonnull(m.player);g_assert_true(m.playing);g_assert_cmpstr(m.title,==,"Faixa de teste");g_assert_cmpstr(m.artist,==,"Artista");
    for(int i=0;i<3;i++){media_action(&m,i);}spin();for(int i=0;i<3;i++){g_assert_cmpint(calls[i],==,1);}
    m.can_next=false;media_action(&m,2);media_action(&m,9);spin();g_assert_cmpint(calls[2],==,1);
    GVariantBuilder b;g_variant_builder_init(&b,G_VARIANT_TYPE_VARDICT);g_variant_builder_add(&b,"{sv}","PlaybackStatus",g_variant_new_string("Paused"));g_dbus_connection_emit_signal(bus,NULL,"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","PropertiesChanged",g_variant_new("(sa{sv}as)","org.mpris.MediaPlayer2.Player",&b,NULL),NULL);spin();g_assert_false(m.playing);
    reply=g_dbus_connection_call_sync(bus,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","ReleaseName",g_variant_new("(s)","org.mpris.MediaPlayer2.JLinuxTest"),NULL,0,2000,NULL,NULL);g_assert_nonnull(reply);g_variant_unref(reply);spin();g_assert_null(m.player);g_assert_false(m.can_play);
    media_finish(&m);
    media_init(&m);media_finish(&m); /* Cancellation before the bus callback. */
    g_dbus_connection_unregister_object(bus,object);g_dbus_node_info_unref(node);g_object_unref(bus);return 0;
}
