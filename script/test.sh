#!/bin/bash
computeNode=$1
indexCacheSize=${2:-50}
message=$3

time=$(date +%F-%T)
echo ${time}


ouput_dir="./log/${time}-cNode${computeNode}-cache${indexCacheSize}-${message}"
mkdir -p ${ouput_dir}
ssh cjq@192.168.6.7 "cd /home/cjq/build2/ && mkdir -p ${ouput_dir}"

../build/benchmark_memory 1 ${computeNode} > ${ouput_dir}/memory 2>&1 &

for ((i=1; i<=${computeNode}; i++))
do
#ssh cjq@192.168.6.7 "cd /home/cjq/build2 && ./benchmark ${computeNode} 100 8 ${indexCacheSize} > ${ouput_dir}/compute${i} 2>&1 &" &
../build/benchmark ${computeNode} 0 8 ${indexCacheSize} > ${ouput_dir}/compute${i} 2>&1 &
if [ $i == 1 ]
    then sleep 2
fi

done

ssh ubuntu@dpu "cd /home/ubuntu/build && ./dpu_client ${computeNode} 16  > log.txt 2>&1 &" &