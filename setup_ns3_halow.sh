#!/bin/bash
# 1. Preparação do Dockerfile
cat << 'EOF' > Dockerfile
FROM ubuntu:16.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential g++-multilib libqt4-dev qt4-dev-tools flex bison \
    libgsl2 gsl-bin git python nano sudo && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
CMD ["/bin/bash"]
EOF

docker build -t ns3-halow-env .

# 2. Clonagem do Repositório NS-3
if [ ! -d "IEEE-802.11ah-ns-3" ]; then
    git clone https://github.com/imec-idlab/IEEE-802.11ah-ns-3.git
fi

# 3. Correção do Issue #8 (SIGIOT)
CONFIG_FILE="IEEE-802.11ah-ns-3/scratch/rca/Configuration.h"
if [ -f "$CONFIG_FILE" ]; then
    sed -i 's/uint32_t pageSliceLength=6;/uint32_t pageSliceLength=1;/' "$CONFIG_FILE"
    sed -i 's/uint32_t pageSliceCount=2;/uint32_t pageSliceCount=0;/' "$CONFIG_FILE"
fi

# 4. Compilação Base do Simulador (Via Docker)
docker run --rm -v $(pwd)/IEEE-802.11ah-ns-3:/workspace/IEEE-802.11ah-ns-3 ns3-halow-env bash -c "cd IEEE-802.11ah-ns-3 && CXXFLAGS=\"-std=c++11\" ./waf configure --disable-examples --disable-tests && ./waf"

# 5. Clonagem e Configuração do ahVisualizer
if [ ! -d "ahVisualizer" ]; then
    git clone https://github.com/imec-idlab/ahVisualizer.git
fi
cd ahVisualizer/forwardsocketdata
sed -i 's/var io = socket.listen(server);/var io = socket(server);/' index.js
npm install express socket.io