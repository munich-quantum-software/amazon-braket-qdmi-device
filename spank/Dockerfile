# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    libcurl4-openssl-dev \
    libslurm-dev \
    libssl-dev \
    munge \
    slurm-wlm \
    sudo \
    && rm -rf /var/lib/apt/lists/*

COPY --from=ghcr.io/astral-sh/uv:0.11.29 /uv /uvx /usr/local/bin/
ENV UV_PYTHON_INSTALL_DIR=/opt/uv/python
RUN uv python install 3.12

RUN userdel -r ubuntu 2>/dev/null || true
RUN useradd -m -u 1000 -s /bin/bash testuser && \
    echo "testuser ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN install -d -o slurm -g slurm /var/spool/slurmctld && \
    install -d -o root -g root /var/spool/slurmd /etc/slurm/plugstack.conf.d && \
    touch /var/log/slurmctld.log /var/log/slurmd.log && \
    chown slurm:slurm /var/log/slurmctld.log && \
    printf '%s\n' 'include /etc/slurm/plugstack.conf.d/*.conf' \
      > /etc/slurm/plugstack.conf

COPY spank/config/slurm.conf /etc/slurm/slurm.conf
COPY spank/config/cgroup.conf /etc/slurm/cgroup.conf

RUN mkdir -p /workspace && chown testuser:testuser /workspace
WORKDIR /workspace
COPY --chown=testuser:testuser . /workspace
RUN chmod 755 /workspace/spank/test/*.sh

# Install MQT Core from the repository lock so the test uses the same driver
# revision as the project, without introducing another dependency pin here.
RUN UV_PROJECT_ENVIRONMENT=/opt/spank-venv \
    uv sync --locked --only-group test --no-install-project --python 3.12

# Build the standalone plugin and install the provider runtime plus its device
# catalogue for the job-process connector probe.
RUN --mount=type=cache,id=amazon-braket-qdmi-spank-deps,target=/workspace/build-spank-docker/_deps \
    SLURM_LIB_DIR="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/slurm-wlm" && \
    cmake -S /workspace -B /workspace/build-spank-docker \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_AMAZON_BRAKET_TESTS=OFF \
      -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON \
      -DAMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR="$SLURM_LIB_DIR" \
      -DAMAZON_BRAKET_QDMI_SLURM_CONF_DIR=/etc/slurm && \
    cmake --build /workspace/build-spank-docker \
      --target amazon-braket-qdmi-device amazon-braket-qdmi-spank \
      --parallel 2 && \
    cmake --install /workspace/build-spank-docker \
      --component amazon-braket-qdmi-device_Runtime && \
    cmake --install /workspace/build-spank-docker \
      --component amazon-braket-qdmi-spank-plugin && \
    printf '%s\n' \
      "required $SLURM_LIB_DIR/amazon-braket-qdmi-spank.so amazon_braket_profile=administrator-default" \
      > /etc/slurm/plugstack.conf.d/amazon-braket-qdmi.conf

ENV UV_OFFLINE=1
ENV MQT_CORE_QDMI_CONFIG_FILE=/usr/local/lib/amazon-braket-qdmi-device.qdmi.json

USER testuser

ENTRYPOINT ["/workspace/spank/test/entrypoint.sh"]
CMD ["/workspace/spank/test/run_docker_tests.sh"]
