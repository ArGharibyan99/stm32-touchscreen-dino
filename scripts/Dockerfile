FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
    git \
    cmake \
    ninja-build \
    gperf \
    ccache \
    dfu-util \
    device-tree-compiler \
    openocd \
    wget \
    python3 \
    python3-dev \
    python3-pip \
    python3-venv \
    python3-setuptools \
    python3-wheel \
    xz-utils \
    file \
    make \
    gcc \
    gcc-multilib \
    g++-multilib \
    libsdl2-dev \
    libmagic1 \
    udev \
    sudo \
    unzip \
    xz-utils \
    nano \
    vim \
    picocom \
    usbutils \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/zephyr-venv && \
    /opt/zephyr-venv/bin/pip install --upgrade pip setuptools wheel && \
    /opt/zephyr-venv/bin/pip install \
      west \
      patool \
      semver \
      pyelftools \
      PyYAML \
      pykwalify \
      jsonschema \
      packaging \
      requests \
      tqdm \
      intelhex

ENV PATH="/opt/zephyr-venv/bin:${PATH}"

RUN west --version

# Optional non-root user support.
# UID/GID can be overridden at build time if you want.
ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=1000

RUN set -eux; \
    if getent group "${USER_GID}" >/dev/null; then \
        GROUP_NAME="$(getent group "${USER_GID}" | cut -d: -f1)"; \
    else \
        groupadd --gid "${USER_GID}" "${USERNAME}"; \
        GROUP_NAME="${USERNAME}"; \
    fi; \
    if getent passwd "${USER_UID}" >/dev/null; then \
        ACTUAL_USER="$(getent passwd "${USER_UID}" | cut -d: -f1)"; \
    else \
        useradd --uid "${USER_UID}" --gid "${GROUP_NAME}" -m -s /bin/bash "${USERNAME}"; \
        ACTUAL_USER="${USERNAME}"; \
    fi; \
    usermod -aG sudo "${ACTUAL_USER}"; \
    echo "${ACTUAL_USER} ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/${ACTUAL_USER}"

# Helpful shell defaults
RUN echo 'export PATH=/opt/zephyr-venv/bin:$PATH' >> /etc/bash.bashrc && \
    echo 'alias ll="ls -alF"' >> /etc/bash.bashrc

USER ${USER_UID}:${USER_GID}

CMD ["/bin/bash"]