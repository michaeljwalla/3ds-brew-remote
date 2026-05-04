import time
import src.receiver as receiver

receiver.set_timeline(print)
receiver.start(input("address: ") or "127.0.0.1", displayLive=True)

time.sleep(5)
receiver.stop()
print("Stop")
