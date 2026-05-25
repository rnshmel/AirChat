#!/bin/bash
# Exit immediately if a command exits with a non-zero status
set -e

IMAGE_NAME="airchat-v2"
TAG="latest"
BUILDER_NAME="airchat-builder"

echo "AIRCHAT V2 : DOCKER BUILDX PIPELINE"

# Verify buildx is installed and available
if ! docker buildx version > /dev/null 2>&1; then
    echo "[ERR] Docker 'buildx' plugin not found."
    echo "[*] Please update Docker Desktop or install the 'docker-buildx-plugin' package."
    exit 1
fi

# Create or select a dedicated buildx instance
if ! docker buildx inspect $BUILDER_NAME > /dev/null 2>&1; then
    echo "[*] Creating new buildx instance: $BUILDER_NAME."
    docker buildx create --name $BUILDER_NAME --use
else
    echo "[*] Using existing buildx instance: $BUILDER_NAME."
    docker buildx use $BUILDER_NAME
fi

# Bootstraps the builder (starts the underlying buildkitd container)
docker buildx inspect --bootstrap

# Execute the Build
echo "[*] Compiling frontend assets and building Python backend."

docker buildx build \
    -f Dockerfile \
    --tag ${IMAGE_NAME}:${TAG} \
    --load \
    ../src

echo "Build complete: ${IMAGE_NAME}:${TAG}"
