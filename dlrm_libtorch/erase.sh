#!/usr/bin/env bash
# nvm_bulk_erase.sh
# 逐一將 (a=0..7, b=0..3, c=0..29, d=0) 轉址後執行 erase
# 失敗不會退出；會統計成功/失敗/無效 val

DEV="/dev/nvme0n1"
LOG="nvm_bulk_erase_$(date +%Y%m%d_%H%M%S).log"

# 安全起見，不用 set -e；避免任何單點錯誤中止整批
set -uo pipefail

total=0
ok=0
fail=0
noval=0
zeroval=0

# echo "# Start bulk erase on $DEV @ $(date)" | tee -a "$LOG"

for a in {0..7}; do
  for b in {0..3}; do
    for c in {0..29}; do
      d=0
      ((total++))
      echo "==> (a=$a, b=$b, c=$c, d=$d)" | tee -a "$LOG"

      # 1) 產生位址
      out=$(sudo nvm_addr s20_to_gen "$DEV" "$a" "$b" "$c" "$d" 2>&1)
      #echo "--- nvm_addr output ---" | tee -a "$LOG"
      #echo "$out" | tee -a "$LOG"

      # 2) 解析 val（抓到 'val: 0x...' 的第一個十六進位值）
      val=$(printf '%s\n' "$out" \
        | grep -oE 'val:\s*0x[0-9a-fA-F]+' \
        | head -n1 \
        | sed -E 's/.*(0x[0-9a-fA-F]+)/\1/')

      if [[ -z "${val:-}" ]]; then
     #   echo "!! 無法解析 val，略過 (a=$a b=$b c=$c d=$d)" | tee -a "$LOG"
        ((noval++))
        continue
      fi

      if [[ "$val" =~ ^0x0+$ ]]; then
    #    echo "!! 取得全零 val ($val)，可能裝置/參數無效，仍略過" | tee -a "$LOG"
        ((zeroval++))
        continue
      fi

   #   echo "-> ERASE: sudo nvm_cmd erase $DEV $val" | tee -a "$LOG"

      # 3) 執行 erase（失敗也不中止）
      if sudo nvm_cmd erase "$DEV" "$val" 2>&1 | tee -a "$LOG" ; then
  #      echo "-- OK" | tee -a "$LOG"
        ((ok++))
      else
 #       echo "-- FAIL (繼續下一個)" | tee -a "$LOG"
        ((fail++))
      fi

      # 視需要加點間隔，避免過快：取消註解即可
      # sleep 0.05
    done
  done
done

#echo "# Done @ $(date)" | tee -a "$LOG"
#echo "# Summary: total=$total ok=$ok fail=$fail noval=$noval zeroval=$zeroval" | tee -a "$LOG"
#echo "# Log saved to: $LOG"

