sudo mst status -v # 查看设备名
ibstat mlx5_0 # 查看设备连接方式
mlnx-sf -a show
ibstat # 查看 InfiniBand 端口信息
ibv_devices # 查看设备状态
ibstatus # 查看网卡连接状态
sudo mlxconfig -d /dev/mst/mt41692_pciconf0.1 s INTERNAL_CPU_MODEL=0
sudo mlxconfig -d /dev/mst/mt41692_pciconf0  s INTERNAL_CPU_MODEL=0
sudo mlxconfig -d /dev/mst/mt41692_pciconf0 q
 mlxconfig -d /dev/mst/mt41692_pciconf0  set LINK_TYPE_P1=2 LINK_TYPE_P2=2
virtual Ethernet device called tmfifo_net0 This virtual Ethernet can be thought of as a peer-to-peer tunnel connection between the host and the DPU OS.

mlxconfig -d /dev/mst/mt41692_pciconf0 query

ip addr flush dev 

ibv_devices -d mlx5_0 -v #查看设备详细信息

show_gids mlx5_0 # 查看设备可用端口, gid_index, rmda版本

ib_write_bw -d mlx5_0 
ovs-vsctl show

/opt/mellanox/collectx/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/share/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig

# 大内存
sudo vi /etc/default/grub
GRUB_CMDLINE_LINUX="default_hugepagesz=2M hugepagesz=2M hugepages=512"
sudo update-grub



cat /proc/meminfo | grep Huge
sudo apt install librdmacm-dev

sshfs ubuntu@192.168.100.2:/opt/mellanox/doca ./dpu_doca


git config --global user.name "CJQ"
git config --global user.email "759713112@qq.com"


sudo /etc/init.d/openibd restart


# 设置网卡NAT
ip addr add 192.168.100.1/24 dev tmfifo_net0

echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
iptables -t nat -A POSTROUTING -o eno8303 -j MASQUERADE

echo "nameserver 210.34.0.14" | sudo tee /etc/resolv.conf # in dpu

# 解决查看不到网卡
cat /etc/mellanox/mlnx-sf.conf
sudo devlink dev eswitch set pci/0000:03:00.0 mode switchdev
/sbin/mlnx-sf --action create --device 0000:03:00.1 --sfnum 0 --hwaddr 02:8b:43:b8:4b:8b



mlnx_tune


flint -d /dev/mst/mt41692_pciconf0  -i ./fw-BlueField-3-rel-32_39_2048-900-9D3B6-00SV-A_Ax-NVME-20.4.1-UEFI-21.4.13-UEFI-22.4.12-UEFI-14.32.17-FlexBoot-3.7.300.signed.bin burn 