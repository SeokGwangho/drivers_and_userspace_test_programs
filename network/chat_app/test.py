#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Echo client program
"""

import socket
import time
import datetime

HOST = 'localhost'
PORT = 8787

def main():
    lst = []
    n = 0
    try:
      for i in range(1):
       s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
       s.connect((HOST, PORT))
       lst.append(s)
       n += 1
    except:
      print "n=" + str(n)
    num = 0;
    while True:
        #data = raw_input('> ')
        num += 1
        data = time.strftime("%Y-%m-%d %H:%M:%S",time.localtime(time.time())) + "." + str(datetime.datetime.now().microsecond / 1000)
        if not data:
            break
        s.sendall(data)
        time.sleep(1)
        #data = s.recv(1024)
        #if not data:
        #    break

        print('{}'.format(data))
    #s.close()

if __name__ == '__main__':
    main()
