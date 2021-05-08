#include <gst/gst.h>

int
main (int argc, char *argv[])
{
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;

  /* Initialize GStreamer */
  /*
   * 1、初始化所有内部结构；
   * 2、检查可用的插件
   * 3、在gstreamer内部执行命令行参数
   */
  gst_init (&argc, &argv);

  /* Build the pipeline */
  /* 
   * pipelne：所有相互联系的元素集合
   * gst_parse_launch：快速建立pipeline管道
   * 通过文本解析方式生成一个由单个element（playbin）组成
   * 的pipeline
   */
  pipeline =
      gst_parse_launch
      ("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm",
      NULL);

  /* Start playing */
  /* 整个pipeline设置为GST_STATE_PLAYING状态，启动播放 */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait until error or EOS */
  /*
   * 1、获取管道的总线
   * 2、阻塞等待总线发送来的特定消息：GST_MEESAGE_ERROR和GST_MESSAGE_EOS
   */
  bus = gst_element_get_bus (pipeline);
  msg =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  /* 设置pipeline为NULL状态，以便释放pipelin时可以释放全部资源 */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
