#!/bin/bash
ps aux|grep benchmark |awk '{print $2}'|xargs kill -9
ps -ef | grep dpu_client | awk '{print $2}' | xargs kill -9
ssh ubuntu@dpu "ps -ef | grep dpu_client | awk '{print \$2}' | xargs kill -9"
ssh cjq@192.168.6.7 "ps -ef | grep benchmark | awk '{print \$2}' | xargs kill -9"
addr=$(head -1 ../memcached.conf)
port=$(awk 'NR==2{print}' ../memcached.conf)

# kill old me
cat /tmp/memcached.pid | xargs kill

# launch memcached
memcached -u root -l ${addr} -p  ${port} -c 10000 -d -P /tmp/memcached.pid
sleep 1

# init 
echo -e "set serverNum 0 0 1\r\n0\r\nquit\r" | nc ${addr} ${port}
echo -e "set computeNum 0 0 1\r\n0\r\nquit\r" | nc ${addr} ${port}
echo -e "set dpuNum 0 0 1\r\n0\r\nquit\r" | nc ${addr} ${port}


