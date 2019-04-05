client python test.py Testcase1
server1 python ../test.py Testcase2
server2 python ../test.py Testcase3
vhost1 ifconfig vhost1-eth2 down
sh echo 'Info: vhost1-eth2 down'
sh echo 'Info: Sleep 60 seconds, wait for routing table converging'
sh sleep 60
client python test.py Testcase4
server1 python ../test.py Testcase5
server2 python ../test.py Testcase6
vhost2 ifconfig vhost2-eth3 down
sh echo 'Info: vhost2-eth3 down'
sh echo 'Info: Sleep 60 seconds, wait for routing table converging'
sh sleep 60
client python test.py Testcase7
server1 python ../test.py Testcase8
server2 python ../test.py Testcase9
vhost2 ifconfig vhost2-eth3 up
vhost1 ifconfig vhost1-eth2 up
sh echo 'Info: vhost2-eth3 up'
sh echo 'Info: vhost1-eth2 up'
sh echo 'Info: Sleep 60 seconds, wait for routing table converging'
sh sleep 60
client python test.py Testcase10
server1 python ../test.py Testcase11
server2 python ../test.py Testcase12
vhost1 ifconfig vhost1-eth1 down
sh echo 'Info: vhost1-eth1 down'
sh echo 'Info: Sleep 60 seconds, wait for routing table converging'
sh sleep 60
server1 python ../test.py Testcase13
server2 python ../test.py Testcase14
vhost1 ifconfig vhost1-eth1 up
sh echo 'Info: vhost1-eth1 up'
sh echo 'Info: Sleep 60 seconds, wait for routing table converging'
sh sleep 60
client python ./test.py Testcase15
