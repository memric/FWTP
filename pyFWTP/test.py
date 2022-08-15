from fwtp import FWTP

sender = FWTP('127.0.0.1')

print('Nope test: ', sender.nope())

print('Start test: ', sender.start(0x10, 1024))

data = "Test data to send"
print('Write test: ', sender.write(0x10, 512, 0, len(data), bytearray(data, 'ascii')))

print('Stop test: ', sender.stop(0x10))
