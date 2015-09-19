#!/bin/sh

# 実行時に指定された引数の数、つまり変数 $# の値が 3 でなければエラー終了。
if [ $# -ne 3 ]; then
  echo "指定された引数は$#個です。" 1>&2
  echo "実行するには3個の引数が必要です。" 1>&2
  exit 1
fi

DEVICE_ID=$1
TRIGGER_TIME=$2
STA_LTA=$3

echo "${DEVICE_ID} の揺れを ${TRIGGER_TIME} に検知" > ${TRIGGER_TIME}.txt
echo "STA/LTAの比は、 ${STA_LTA} でした" >> ${TRIGGER_TIME}.txt

# Tiwtter投稿プログラムを実行
/usr/bin/php /home/pi/tw/twitter.php ${DEVICE_ID} ${TRIGGER_TIME} ${STA_LTA}


exit 0
