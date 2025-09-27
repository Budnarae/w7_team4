#pragma once

/**
 * @brief Occlusion Culling �� ����ϴ� Ŭ����
 */
class UStaticMeshComponent;

class COcclusionCuller
{
public:
    COcclusionCuller();

    /**
     * @brief �ø� ���μ����� ���� ȯ���� �ʱ�ȭ�ϰ� View/Projection ����� ����.
     * �� ������ �ø��� �����ϱ� ���� ȣ��Ǿ�� ��
     */
    void InitializeCuller(const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix);

     /**
     * @brief ��Ŭ���� �ø��� ��ü ���μ����� �����ϰ� ���� ���� ������Ʈ ����� ��ȯ
     * @param AllStaticMeshes �������� �ø��� ����� ��� ����ƽ �޽� ���
     * @param CameraPos ���� ī�޶� ��ġ
     * @return �������Ǿ�� �� UStaticMeshComponent ���
     */
    TArray<TObjectPtr<UStaticMeshComponent>> PerformCulling(const TArray<TObjectPtr<UStaticMeshComponent>>& AllStaticMeshes, const FVector& CameraPos);

    // Constants
    static constexpr int Z_BUFFER_WIDTH = 256;
    static constexpr int Z_BUFFER_HEIGHT = 256;
    static constexpr int Z_BUFFER_SIZE = Z_BUFFER_WIDTH * Z_BUFFER_HEIGHT;

private:
    /**
     * @brief �ش� �޽� ������Ʈ�� Z-Buffer�� ���� ���������� �׽�Ʈ�մϴ�.
     * @return �ٿ�� �ڽ��� �ڳ� �� �ϳ��� ���̸� true�� ��ȯ�մϴ�.
     */
    bool IsMeshVisible(const UStaticMeshComponent* MeshComp) const;

    struct BatchProjectionResult BatchProject4(const struct BatchProjectionInput& Input) const;

    /**
     * @brief World Pos > Clip Space > Screen Coordinate
     * @return ��ũ�� ��ǥ (X, Y)�� NDC Depth (Z)�� ���� FVector
     */
    FVector Project(const FVector& WorldPos) const;

    /**
     * @brief CPU Z-Buffer�� �ﰢ���� �����Ͷ���¡, ���� �׽�Ʈ ����
     */
    void RasterizeTriangle(const FVector& P1, const FVector& P2, const FVector& P3, TArray<float>& ZBuffer);

    /**
     * @brief PrimitiveComponent�� AABB�� 12���� �ﰢ�� �������� ��ȯ
     */
    TArray<FVector> ConvertAABBToTriangles(const class UPrimitiveComponent* PrimitiveComp) const;
     /**
     * @brief ȭ�� ���� ���� �� �Ÿ��� �������� ��Ŭ��� ������ �� ��ü ���� ����
     * @param AllCandidates �������� �ø��� ����� ��� ����ƽ �޽� �ĺ� ���
     * @param MaxOccluders Z-Buffer ������ ����� �ִ� ��Ŭ��� ��
     * @return ��Ŭ����� ������ UStaticMeshComponent ���
     */
    TArray<UStaticMeshComponent*> SelectOccluders(const TArray<TObjectPtr<UStaticMeshComponent>>& AllCandidates, const FVector& CameraPos, uint32 MaxOccluders = 100);

    void RasterizeOccluders(const TArray<UStaticMeshComponent*>& SelectedOccluders);

    TArray<float> CPU_ZBuffer;
    FMatrix CurrentViewProj;

};

struct FOccluderScore
{
    UStaticMeshComponent* MeshComp;
    float Score;

    bool operator>(const FOccluderScore& Other) const { return Score > Other.Score; }
};

struct alignas(16) BatchProjectionInput
{
    __m128 WorldX, WorldY, WorldZ; // 4�� ���� X, Y, Z ��ǥ
};

struct alignas(16) BatchProjectionResult
{
    __m128 ScreenX, ScreenY, ScreenZ; // 4�� ���� ��ũ�� ��ǥ
    __m128i PixelX, PixelY;          // ������ �ȼ� ��ǥ
    __m128 InBoundsMask;             // ȭ�� ��� ���� ����ũ
};
