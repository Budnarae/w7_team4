#pragma once

namespace Optimization
{
	/* Primitive�� �߰��� �˾ƾ� �ϴ� �ٸ� ��ü�鿡�� �˸���. */
	void NotifyPrimitiveAdditionToOthers();

	/* Primitive�� ������ �˾ƾ� �ϴ� �ٸ� ��ü�鿡�� �˸���. */
	void NotifyPrimitiveDeletionToOthers();

	/* Primitive�� ���� ������ �˾ƾ� �ϴ� �ٸ� ��ü�鿡�� �˸���. */
	void NotifyPrimitiveDirtyToOthers();
}