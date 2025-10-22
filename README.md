# Week7 조명 시스템 구현 기술 문서

## 1. 개요

본 프로젝트는 게임 엔진에서 실시간 조명 시스템을 구현한 결과물입니다. Forward Shading을 기반으로 다양한 조명 타입과 최적화 기법을 적용하여, 실시간으로 다수의 동적 조명을 효율적으로 처리할 수 있는 렌더링 파이프라인을 구축하였습니다.

### 주요 구현 내용
- **조명 시스템**: Ambient, Directional, Point, Spot Light 구현
- **셰이딩 모델**: Gouraud, Lambert, Blinn-Phong 지원
- **최적화**: Tile-based Light Culling을 통한 성능 개선
- **고급 기능**: Normal Mapping, Uber Shader 시스템

---

## 2. 조명 시스템 구조

### 2.1 조명 컴포넌트 계층 구조

프로젝트는 컴포넌트 패턴을 사용하여 조명 시스템을 설계했습니다.

```
ULightComponentBase (기본 클래스)
├── ULightComponent (기본 조명)
├── UAmbientLightComponent (환경광)
├── UDirectionalLightComponent (방향광)
├── UPointLightComponent (점광원)
└── USpotLightComponent (스포트라이트)
```

**ULightComponentBase** (`LightComponentBase.h`)
모든 조명의 공통 속성을 정의합니다:
- `Intensity`: 조명의 밝기
- `LightColor`: 조명 색상 (RGB)
- `bVisible`: 조명 활성화 여부

각 조명 타입은 이 기본 클래스를 상속하여 고유한 특성을 추가합니다.

### 2.2 조명 타입별 특징

#### Ambient Light (환경광)
전역적으로 모든 오브젝트에 균일하게 적용되는 기본 조명입니다. 그림자나 방향 없이 장면의 최소 밝기를 보장합니다.

#### Directional Light (방향광)
태양광처럼 무한히 먼 거리에서 오는 평행한 빛을 표현합니다. 위치가 아닌 방향만으로 정의되며, 그림자를 위한 직교 투영을 사용합니다.

#### Point Light (점광원)
전구처럼 한 점에서 모든 방향으로 빛을 발산합니다. 거리에 따른 감쇠(Attenuation)와 반경(Radius)으로 영향 범위를 제어합니다.

**주요 속성**:
- `AttenuationRadius`: 빛이 도달하는 최대 거리
- `LightFalloffExponent`: 거리 감쇠 지수

#### Spot Light (스포트라이트)
무대 조명처럼 특정 방향으로 원뿔 형태의 빛을 발산합니다.

**주요 속성**:
- `InnerConeAngle`: 빛이 최대 강도로 비추는 내부 각도
- `OuterConeAngle`: 빛이 완전히 사라지는 외부 각도

두 각도 사이에서 부드럽게 빛이 감쇠하여 자연스러운 조명 효과를 만듭니다.

---

## 3. Uber Shader 시스템

### 3.1 개념

Uber Shader는 하나의 셰이더 파일에서 여러 조명 모델과 기능을 지원하는 통합 셰이더입니다. 전처리기 매크로를 활용하여 필요한 기능만 선택적으로 컴파일합니다.

**장점**:
- 셰이더 파일 관리 간소화
- 런타임에서 조명 모델 전환 가능
- 코드 재사용성 향상

### 3.2 지원하는 셰이딩 모델

#### Gouraud Shading
정점 셰이더에서 조명 계산을 수행하고, 픽셀 셰이더에서는 보간된 결과만 사용합니다. 계산량이 적지만 품질이 낮습니다.

#### Lambert Shading
픽셀 단위로 난반사(Diffuse) 조명을 계산합니다. 간단하지만 현실적인 표면 표현이 가능합니다.

#### Blinn-Phong Shading
난반사와 정반사(Specular)를 모두 계산하여 광택 있는 표면을 표현합니다. Half Vector를 사용하여 Phong보다 효율적으로 하이라이트를 계산합니다.

### 3.3 셰이더 매크로 시스템

`UberLit.hlsl`에서는 다음과 같은 매크로로 기능을 제어합니다:

```hlsl
#define LIGHTING_MODEL_GOURAUD 0/1
#define LIGHTING_MODEL_LAMBERT 0/1
#define LIGHTING_MODEL_PHONG 0/1
#define HAS_NORMAL_MAP 0/1
```

엔진은 머티리얼 설정에 따라 필요한 매크로 조합으로 셰이더를 컴파일합니다.

---

## 4. Normal Mapping

Normal Map은 텍스처를 통해 표면의 미세한 굴곡을 표현하는 기법입니다. 실제 지오메트리를 추가하지 않고도 디테일한 표면을 렌더링할 수 있습니다.

### 구현 방식

1. **Normal Map 읽기**: RGB 값을 법선 벡터로 변환 (0~1 범위를 -1~1로 매핑)
2. **Tangent Space 변환**: 텍스처 공간의 법선을 월드 공간으로 변환
3. **조명 계산**: 변환된 법선으로 조명 방정식 계산

**Tangent Space**는 표면에 붙어있는 좌표계로, 모델의 회전이나 변형에도 Normal Map이 올바르게 작동하도록 합니다.

---

## 5. Tile-based Light Culling

### 5.1 필요성

Forward Shading에서는 각 픽셀마다 모든 조명을 계산합니다. 조명이 많아질수록 연산량이 급격히 증가하여 성능 저하가 발생합니다.

**문제점**: 100개의 조명 × 1920×1080 픽셀 = 약 2억 번의 조명 계산

### 5.2 해결 방법

화면을 작은 타일(예: 32×32 픽셀)로 나누고, 각 타일에 영향을 주는 조명만 선별하는 기법입니다.

**과정**:
1. **화면 분할**: 화면을 32×32 픽셀 타일로 나눔 (1920×1080 → 60×34 타일)
2. **깊이 분석**: 각 타일의 최소/최대 깊이를 계산하여 3D 범위 결정
3. **조명 선별**: 타일의 3D 범위와 교차하는 조명만 리스트에 추가
4. **인덱스 저장**: 타일별 조명 인덱스를 버퍼에 저장

**결과**: 픽셀 셰이더는 자신이 속한 타일의 조명만 계산하므로 연산량이 크게 감소합니다.

### 5.3 구현 세부사항

`LightCulling.hlsl`에서 Compute Shader로 구현되었습니다:

**입력**:
- Scene Depth Texture (깊이 버퍼)
- Point/Spot Light 정보 배열

**출력**:
- `TileLightOffsets`: 각 타일의 조명 리스트 시작 위치
- `TileLightCounts`: 각 타일의 조명 개수
- `TileLightIndices`: 전역 조명 인덱스 배열

**최적화 기법**:
- Shared Memory를 사용한 타일 내 협력 연산
- Parallel Reduction으로 Min/Max Depth 계산

---

## 6. View Mode 시스템

개발 및 디버깅을 위해 다양한 시각화 모드를 지원합니다.

### 주요 View Mode

- **Lit**: 광원 연산 적용. Gouroud, Lambert, Blinn Phong 중 선택 가능
- **Unlit**: 조명 없이 Base Color만 표시
- **World Normal**: 월드 공간 법선을 색상으로 표시
- **Light Complexity**: Tile-based Light Culling 디버깅을 위해 각 타일의 정보를 시각화.

이를 통해 조명 계산이 올바르게 작동하는지, Normal Map이 제대로 적용되는지 등을 시각적으로 확인할 수 있습니다.

---

## 7. 성능 최적화

### 조명 최적화 전략

1. **Light Culling**: 불필요한 조명 계산 제거
2. **Shader Permutation**: 필요한 기능만 활성화된 셰이더 사용
3. **Constant Buffer**: 프레임당 한 번만 갱신되는 데이터 분리
4. **Structured Buffer**: 동적 조명 배열을 GPU 메모리에 효율적으로 전달

### 렌더링 파이프라인

```
1. Scene Depth 렌더링
2. Light Culling Pass (Compute Shader)
3. Forward Rendering Pass
   ├─ Ambient Light 적용
   ├─ Directional Light 계산
   └─ 타일별 Point/Spot Light 계산
4. Post Processing
```

---

## 8. 사용 방법

### 프로젝트 빌드
1. Visual Studio에서 `W7_GTL.sln` 열기
2. Release 또는 Debug 구성으로 빌드
3. `Build/Release` 또는 `Build/Debug`에서 실행 파일 실행

### 조명 추가하기
에디터에서 Actor를 생성하고 원하는 Light Component를 추가합니다. 각 조명의 Intensity, Color, Radius 등을 조절하여 원하는 조명 효과를 얻을 수 있습니다.

### View Mode 전환
에디터의 View Mode 메뉴에서 다양한 시각화 모드를 선택하여 렌더링 결과를 확인할 수 있습니다.

---

## 10. 프로젝트 구조

```
Week7/
├── Engine/
│   ├── Source/
│   │   ├── Component/Light/     # 조명 컴포넌트 구현
│   │   └── Render/RenderPass/   # 렌더링 패스 (LightPass, LightCullingPass)
│   └── Asset/Shader/
│       ├── UberLit.hlsl         # 통합 조명 셰이더
│       └── LightCulling.hlsl    # 타일 기반 조명 선별 셰이더
└─  Build/                       # 빌드 결과물
```
