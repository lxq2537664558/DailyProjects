
#include <stdio.h>
#include <stdlib.h>

#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#include "FractalRenderer.h"

#if USE_DOUBLE
#define ufmod   fmod
#else
#define ufmod   fmodf
#endif

static const int kBlockSize = 32;

#define cudaCheck(call) { cudaCheckError((call), __FILE__, __LINE__); }
inline void cudaCheckError(cudaError_t error, const char *file, int line, bool abort = true)
{
    if (error != cudaSuccess)
    {
        fprintf(stderr, "CUDA error: %s %s %d\n", cudaGetErrorString(error), file, line);
        if (abort) exit(error);
    }
}

__device__ static int HSV2RGB(TFloat H, TFloat S, TFloat V)
{
    TFloat C = V * S;
    TFloat H1 = H * 6;
    TFloat X = C * (1 - fabs(ufmod(H1, 2.0) - 1));
    TFloat R1, G1, B1;
    switch ((int)floor(H1))
    {
    case 0:
        R1 = C; G1 = X; B1 = 0;
        break;
    case 1:
        R1 = X; G1 = C; B1 = 0;
        break;
    case 2:
        R1 = 0; G1 = C; B1 = X;
        break;
    case 3:
        R1 = 0; G1 = X; B1 = C;
        break;
    case 4:
        R1 = X; G1 = 0; B1 = C;
        break;
    case 5:
        R1 = C; G1 = 0; B1 = X;
        break;
    default:
        R1 = 0; G1 = 0; B1 = 0;
        break;
    }

    TFloat m = V - C;
    int r = (int)((R1 + m) * 255);
    int g = (int)((G1 + m) * 255);
    int b = (int)((B1 + m) * 255);
    return (0 << 24) | (r << 16) | (g << 8) | (b << 0);
}

__global__ void juliaset(int *buffer, int width, int height,
    int maxIteration, TFloat cx, TFloat cy, TFloat minX, TFloat maxX, TFloat minY, TFloat maxY)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    int idx = y * width + x;
    TFloat fx = minX + (maxX - minX) * x / width;
    TFloat fy = minY + (maxY - minY) * y / height;

    TFloat zx = fx;
    TFloat zy = fy;
    TFloat iteration = 0;
    for (; iteration < maxIteration && zx * zx + zy * zy < 4; iteration += 1)
    {
        TFloat newZx = zx * zx - zy * zy + cx;
        zy = 2 * zx * zy + cy;
        zx = newZx;
    }

    if (iteration < maxIteration)
    {
        TFloat logZn = log(zx * zx + zy * zy) / 2;
        TFloat nu = log(logZn / log(2.0)) / log(2.0);
        iteration = iteration + 1 - nu;
    }

    TFloat value = iteration / maxIteration;
    buffer[idx] = HSV2RGB(value, 1 - value * value, sqrt(value));
}

__global__ void mandelbrot(int *buffer, int width, int height,
    int maxIteration, TFloat minX, TFloat maxX, TFloat minY, TFloat maxY)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    int idx = y * width + x;
    TFloat fx = minX + (maxX - minX) * x / width;
    TFloat fy = minY + (maxY - minY) * y / height;

    TFloat zx = 0;
    TFloat zy = 0;
    TFloat iteration = 0;
    for (; iteration < maxIteration && zx * zx + zy * zy < 4; iteration += 1)
    {
        TFloat newZx = zx * zx - zy * zy + fx;
        zy = 2 * zx * zy + fy;
        zx = newZx;
    }

    if (iteration < maxIteration)
    {
        TFloat logZn = log(zx * zx + zy * zy) / 2;
        TFloat nu = log(logZn / log(2.0)) / log(2.0);
        iteration = iteration + 1 - nu;
    }

    TFloat value = iteration / maxIteration;
    buffer[idx] = HSV2RGB(value, 1 - value * value, sqrt(value));
}

class CUDAFractalRenderer : public IFractalRenderer
{
public:
    CUDAFractalRenderer(int *buffer, int width, int height)
    {
        cudaCheck(cudaMalloc(&mDeviceBuffer, width * height * sizeof(*mDeviceBuffer)));
    }

    ~CUDAFractalRenderer()
    {
        cudaCheck(cudaFree(mDeviceBuffer));
    }

    virtual void ResetBuffer(int *buffer, int width, int height)
    {
        cudaCheck(cudaFree(mDeviceBuffer));
        cudaCheck(cudaMalloc(&mDeviceBuffer, width * height * sizeof(*mDeviceBuffer)));
    }

    virtual void RenderMandelbrot(
        int *buffer, int width, int height, int maxIteration,
        TFloat minX, TFloat maxX, TFloat minY, TFloat maxY)
    {
        dim3 dimBlock(kBlockSize, kBlockSize);
        dim3 dimGrid((width + dimBlock.x - 1) / dimBlock.x, (height + dimBlock.y - 1) / dimBlock.y);
        mandelbrot << <dimGrid, dimBlock >> >(mDeviceBuffer, width, height, maxIteration, minX, maxX, minY, maxY);

        cudaCheck(cudaMemcpy(buffer, mDeviceBuffer, width * height * sizeof(*mDeviceBuffer), cudaMemcpyDeviceToHost));        
    }

    virtual void RenderJuliaSet(
        int *buffer, int width, int height, int maxIteration, TFloat cx, TFloat cy,
        TFloat minX, TFloat maxX, TFloat minY, TFloat maxY)
    {
        dim3 dimBlock(kBlockSize, kBlockSize);
        dim3 dimGrid((width + dimBlock.x - 1) / dimBlock.x, (height + dimBlock.y - 1) / dimBlock.y);
        juliaset << <dimGrid, dimBlock >> >(mDeviceBuffer, width, height, maxIteration, cx, cy, minX, maxX, minY, maxY);

        cudaCheck(cudaMemcpy(buffer, mDeviceBuffer, width * height * sizeof(*mDeviceBuffer), cudaMemcpyDeviceToHost));
    }

private:
    int mWidth, mHeight;
    int *mDeviceBuffer;
};

IFractalRenderer* CreateCUDAFractalRenderer(int *buffer, int width, int height)
{
    return new CUDAFractalRenderer(buffer, width, height);
}