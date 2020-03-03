#!/usr/bin/env python3

import sys, time, serial, argparse

CMD_INIT    = '$$$'
CMD_OK      = 'OK'
CMD_FAIL    = 'FAIL'
CMD_UNKNOWN = 'UNKNOWN'
CMD_ABORT   = 'ABORT'
CMD_DT      = 'DT'

def receiveResponse(sp):
    tmp = sp.readline()
    tmp = tmp.decode("utf-8")

    length = len(tmp)
    if length < 2:
        return tmp

    count = 0
    if tmp[-1] == '\n':
        count += 1
    if tmp[-2] == '\r':
        count += 1

    return tmp[:length - count]

def sendCommand(sp, cmd, *args):
    res = cmd.encode('utf-8')
    count = len(args)
    if count > 0:
        res += ' '.encode('utf-8')
        for i in range(count): 
            res += str(args[i]).encode('utf-8')
            if i < count - 1:
                res += b','

    sp.write(res + b'\r\n')

def main():
    with serial.Serial(options.port, options.speed, timeout=options.timeout) as sp:
        print("Awaiting command mode window...")
        rsp = ''
        while rsp != '$$$':
            rsp = receiveResponse(sp)
            #print(rsp)
        print(rsp)

        print("Entering command mode...")
        sendCommand(sp, '$$$')
        rsp = receiveResponse(sp)
        print(rsp)
        if rsp != CMD_OK:
            print("Error: Could not enter command mode: \'{}\'".format(rsp))
            sys.exit(1)
        print(rsp)

        dt = int(time.time())
        print("Updating date/time to \'{}\'...".format(dt))
        sendCommand(sp, CMD_DT, dt)
        rsp = receiveResponse(sp)
        if rsp[:2] != CMD_OK:
            print("Error: Could not update date/time mode: \'{}\'".format(rsp))
            sys.exit(1)
        print(rsp)

        print("Exiting command mode...")
        sendCommand(sp, '$$$')
        rsp = receiveResponse(sp)
        if rsp != CMD_OK:
            print("Error: Could not exit command mode: \'{}\'".format(rsp))
            sys.exit(1)
        print(rsp)

    sys.exit()

if __name__== "__main__":
    options = argparse.ArgumentParser(
        description="",
        epilog=""
    )

    options.add_argument(dest='port', type=str, help="Port name", metavar='port')
    options.add_argument('-s', '--speed', dest='speed', type=int, required=False, default=9600, help="Baud rate", metavar='speed')
    options.add_argument('-t', '--timeout', dest='timeout', type=float, required=False, default=1.0, help="Timeout (seconds)", metavar='timeout')
    options = options.parse_args()

    main()
