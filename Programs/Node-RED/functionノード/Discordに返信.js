let payload = "<" + msg.my_device_id + ">が、OK, 受け付けました！\r\n------\r\n" + msg.content
msg.payload = payload
return msg;