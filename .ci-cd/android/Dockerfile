# Android (arm64-v8a / x86_64) APK build environment based on JDK 17 + Android SDK 34 + NDK 27.2 + SDL3 3.4.18 + Gradle 8.7.
FROM eclipse-temurin:17-jdk-noble

ARG CMDLINE_TOOLS_URL=https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
ARG SDL3_VERSION=3.4.18
ARG GRADLE_VERSION=8.7

ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV SDL3_ANDROID_DIR=/opt/SDL3
ENV PATH=/opt/gradle/bin:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:${PATH}

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates curl unzip python3 git ninja-build make && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p ${ANDROID_HOME}/cmdline-tools && \
    curl -fsSL "${CMDLINE_TOOLS_URL}" -o /tmp/cmdtools.zip && \
    unzip -q /tmp/cmdtools.zip -d ${ANDROID_HOME}/cmdline-tools && \
    mv ${ANDROID_HOME}/cmdline-tools/cmdline-tools ${ANDROID_HOME}/cmdline-tools/latest && \
    rm /tmp/cmdtools.zip

RUN yes | sdkmanager --licenses >/dev/null && \
    sdkmanager --install \
        "platforms;android-34" \
        "build-tools;34.0.0" \
        "ndk;27.2.12479018" \
        "cmake;3.22.1"

RUN curl -fsSL "https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip" -o /tmp/gradle.zip && \
    unzip -q /tmp/gradle.zip -d /opt && \
    mv /opt/gradle-${GRADLE_VERSION} /opt/gradle && \
    rm /tmp/gradle.zip

RUN curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-${SDL3_VERSION}.tar.gz" | \
    tar -xz -C /opt && \
    mv /opt/SDL3-${SDL3_VERSION} /opt/SDL3
