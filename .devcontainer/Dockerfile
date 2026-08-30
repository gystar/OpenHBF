FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=1000

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Common OpenHbf development environment. Add project-specific system
# dependencies here as more simulators are integrated into the workspace.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        clangd \
        clang-format \
        clang-tidy \
        cmake \
        curl \
        gdb \
        git \
        lld \
        lldb \
        ninja-build \
        pkg-config \
        python-is-python3 \
        python3 \
        python3-dev \
        python3-pip \
        python3-venv \
        ripgrep \
        sudo \
    && rm -rf /var/lib/apt/lists/*

RUN existing_user="$(getent passwd "${USER_UID}" | cut -d: -f1 || true)" \
    && if [[ -n "${existing_user}" && "${existing_user}" != "${USERNAME}" ]]; then \
         usermod --login "${USERNAME}" --home "/home/${USERNAME}" --move-home "${existing_user}"; \
         existing_group="$(id -gn "${USERNAME}")"; \
         if [[ "${existing_group}" != "${USERNAME}" ]]; then groupmod --new-name "${USERNAME}" "${existing_group}"; fi; \
       elif [[ -z "${existing_user}" ]]; then \
         groupadd --gid "${USER_GID}" "${USERNAME}"; \
         useradd --uid "${USER_UID}" --gid "${USER_GID}" --create-home --shell /bin/bash "${USERNAME}"; \
       fi \
    && echo "${USERNAME} ALL=(root) NOPASSWD:ALL" > "/etc/sudoers.d/${USERNAME}" \
    && chmod 0440 "/etc/sudoers.d/${USERNAME}"

# A single generic Python environment shared by projects in this container.
ENV VIRTUAL_ENV=/opt/venv
ENV PATH="${VIRTUAL_ENV}/bin:${PATH}"

RUN python3 -m venv "${VIRTUAL_ENV}" \
    && chown -R "${USERNAME}:${USERNAME}" "${VIRTUAL_ENV}" \
    && echo 'if [ -f "${VIRTUAL_ENV}/bin/activate" ]; then . "${VIRTUAL_ENV}/bin/activate"; fi' \
       >> "/home/${USERNAME}/.bashrc"

WORKDIR /workspace
CMD ["sleep", "infinity"]
