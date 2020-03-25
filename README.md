csn_raspi
=========

## outpu files
### csn.log
- 連続波形のデータを記録したファイル
- syslogを使用してrotate
  - rotationは，1時間 or 25MBを超えたら
  - csn_log_%Y%m%d-%s で保存
  - 終了時刻がファイル名

### inpファイル
- 地震をトリガーした場合に作成されるファイル
- デバイスID_トリガー時刻.inp
- トリガーした時刻の前10秒 + 後ろ60秒分のデータ