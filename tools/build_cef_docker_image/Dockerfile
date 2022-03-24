# Based on https://magpcss.org/ceforum/viewtopic.php?f=7&t=17776&p=46448#p47371

FROM ubuntu:bionic
USER root
SHELL ["/bin/bash", "-c"]
COPY install/ /app/install
RUN chmod +x /app/install/*.sh && /app/install/install.sh
RUN groupadd -g 999 appuser && useradd -r -u 999 -g appuser appuser && mkdir /home/appuser && chown appuser:appuser /home/appuser
USER appuser
