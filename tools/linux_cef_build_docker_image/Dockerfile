FROM debian:latest

# Dockerfile from
# https://magpcss.org/ceforum/viewtopic.php?f=7&t=17776&p=46448#p47371
# linked from official automated build setup documentation in
# https://bitbucket.org/chromiumembedded/cef/wiki/AutomatedBuildSetup.md

# Required otherwise the following RUN instruction would fail at the first chmod
# with 'missing operand'. See https://serverfault.com/a/960335/320690
USER root
SHELL ["/bin/bash", "-c"]

# Copy the install directory into the image.
COPY install/ /app/install

# Install Chromium dependencies.
RUN chmod +x /app/install/*.sh && \
    /app/install/install.sh

# Continue to the default entrypoint as a non-root user.
RUN groupadd -g 999 appuser && \
    useradd -r -u 999 -g appuser appuser
USER appuser
