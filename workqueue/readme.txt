ドライバファイル名は、workqueue.cを使用しないこと。
使用すると、

insmod: ERROR: could not insert module workqueue.ko: Invalid parameters
dmesg:
  [   59.203584] workqueue: module is already loaded

と、エラーが出た。

ドライバファイル名を、work_queue.cにすると、治ったが…
