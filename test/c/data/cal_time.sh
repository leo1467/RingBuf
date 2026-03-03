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
    sum=0
    for ele in ${arr[@]}; do
        ((sum=sum+ele))
    done
    av=$((sum/${#arr[@]}))

    sqsum=0
    for ele in ${arr[@]}; do
        diff=$((ele-av))
        sqsum=$((sqsum+diff*diff))
    done
    e=$((sqsum/av)) 
    square_root=$(echo "scale=2; sqrt($e)" | bc -l)
    echo $1:${av}, ${square_root}
}

sd $1