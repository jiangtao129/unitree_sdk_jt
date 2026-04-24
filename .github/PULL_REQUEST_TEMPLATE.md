<!--
PR 标题格式: <type>: <一句话>
type 取自: feat / fix / chore / docs / refactor / ci / test
例: feat: view_map.py 加 --no-window 选项
-->

## Summary
<!-- 1~3 个 bullet, 说"改了什么 / 为什么改", 不要复制 diff -->
- 
- 

## Test plan
<!-- 必须包含 bash scripts/verify.sh 的关键输出 -->
- [ ] `bash scripts/verify.sh` 通过 (本地)
- [ ] CI 通过 (GitHub Actions)
- [ ] 其他验证: <如有, e.g. ssh dock 跑硬件回归>

```
<贴 verify.sh 关键输出: build OK / selftest OK 等>
```

## Risk / 影响面
<!-- 这次改动可能影响哪些模块? 是否需要硬件回归? -->
- 影响模块: 
- 是否需要硬件回归 (Go2 dock): 是 / 否
- 兼容性: 是否破坏现有调用方式 / 配置 / 数据格式

## Related
<!-- linked issue / 相关 PR / 相关计划 doc -->
- 
