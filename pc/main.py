import src.receiver as receiver

receiver.set_timeline(print)
receiver.start("127.0.0.1", displayLive=True)