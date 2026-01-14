#!/usr/bin/env bash

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

function find_seq_not_right {
    readarray arr < ${DIR}/y
    for ele in ${arr[@]}; do
        nums=($(grep -o '[0-9]\+' <<< ${ele}))
        if [[ ${nums[0]} -ne ${nums[1]} && ${nums[1]} -ne ${nums[2]} ]]; then
            echo ${ele}
        fi
    done
}

function average {
    local -a arr
    readarray arr < ${DIR}/$1
    sum=0
    for ele in ${arr[@]}; do
        ((sum=sum+ele))
    done
    local av=$((sum/${#arr[@]}))
    echo $1:${av}
}

function sd {
    local -a arr
    readarray arr < ${DIR}/$1

    # 計算平均值
    sum=0
    for ele in ${arr[@]}; do
        ((sum=sum+ele))
    done
    av=$((sum/${#arr[@]}))

    # 計算標準差
    sqsum=0
    for ele in ${arr[@]}; do
        diff=$((ele-av))
        sqsum=$((sqsum+diff*diff))
    done

    # 母體標準差
    variance=$((sqsum/${#arr[@]}))
    square_root=$(echo "scale=2; sqrt($variance)" | bc -l)
    #echo $1:${av}, ${square_root}

    len=${#arr[@]}
    echo "分析延遲分佈：" && awk '{if($1 < 200) count_excellent++; else if($1 < 500) count_good++; else if($1 < 1000) count_fair++; else count_poor++} END {printf "< 200ns (excellent): %d %f%\n", count_excellent, count_excellent/'"$len"'*100; printf "200-500ns (good): %d %f%\n", count_good, count_good/'"$len"'*100; printf "500-1000ns (fair): %d %f%\n", count_fair, count_fair/'"$len"'*100; printf "> 1000ns (poor): %d, %f%\n", count_poor, count_poor/'"$len"'*100}' $1
    #echo "分析延遲分佈：" && awk 'NR > 1000 {if($1 < 500) count_low++; else if($1 < 1000) count_mid++; else if($1 < 5000) count_high++; else count_extreme++} END {print "< 500ns:", count_low; print "500-1000ns:", count_mid; print "1000-5000ns:", count_high; print "> 5000ns:", count_extreme}' $1
    echo
    echo "計算基本統計：" && awk 'NR > 1000 {sum+=$1; if($1 > max) max=$1; if(min=="" || $1 < min) min=$1} END {print "平均延遲:", sum/(NR-1000), "ns";print "母體標準差:", '"$square_root"', "ns"; print "最小延遲:", min, "ns"; print "最大延遲:", max, "ns"}' $1
    # echo "檢查高延遲事件 (>1000ns)：" && awk '$1 > 1000 {print NR ":", $1 "ns"}' $1 | head -20
    # echo "檢查後段的穩定狀態延遲分佈：" && tail -2000 $1 | awk '{if($1 < 200) count_excellent++; else if($1 < 500) count_good++; else if($1 < 1000) count_fair++; else count_poor++} END {print "< 200ns (excellent):", count_excellent; print "200-500ns (good):", count_good; print "500-1000ns (fair):", count_fair; print "> 1000ns (poor):", count_poor}'
}

sd $1

