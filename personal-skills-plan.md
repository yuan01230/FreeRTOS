# Personal Skills Plan

这份文档用于规划后续可以创建的个人 Codex skills。它本身不会自动生效；真正生效的是放在 Codex skills 目录里的 `SKILL.md` 文件。

推荐顺序：

1. 先使用 `AGENTS.md` 稳定日常协作风格。
2. 观察一段时间，把反复出现的习惯提炼成 skills。
3. 只有当某个 workflow 需要脚本、模板、外部工具或团队分发时，再考虑做 plugin。

## Skill 1: safe-code-change

用途：让 Codex 在修改代码时自动判断风险等级，并选择合适的执行方式。

适合触发：

- 修 bug
- 添加功能
- 调整配置
- 小范围重构
- 修改测试

建议行为：

- 修改前先阅读相关文件。
- 小改动可以直接执行。
- 涉及架构、依赖、数据库、鉴权、删除文件、批量改动时先说明方案并等待确认。
- 不做无关重构。
- 不回滚用户已有改动。
- 修改后运行相关验证命令。
- 无法验证时说明原因。

可选资源：

- `references/risk-levels.md`：定义低、中、高风险改动标准。
- `references/verification.md`：列出不同项目类型的验证建议。

## Skill 2: chinese-collaboration

用途：让 Codex 更稳定地使用符合用户习惯的中文协作风格。

适合触发：

- 日常问答
- 解释代码
- 写方案
- 总结修改
- 做 code review

建议行为：

- 默认中文。
- 先结论后细节。
- 简洁但不敷衍。
- 对简单任务直接给答案。
- 对复杂任务先给简短计划。
- 主动指出更好的做法，但不要扩大任务范围。
- 少用空泛鼓励，多给可执行建议。

可选资源：

- `references/response-style.md`：整理常用输出模板，比如完成总结、风险说明、review 格式。

## Skill 3: preflight-check

用途：在任务完成、准备提交、或用户要求“检查一下”时，做最后一轮工程检查。

适合触发：

- 准备提交
- 准备开 PR
- 用户说“检查一下”
- 用户说“看看还有没有问题”
- 用户说“准备收尾”

建议行为：

- 查看当前改动范围。
- 确认是否有无关文件被修改。
- 确认是否有用户已有改动，避免误覆盖。
- 运行相关测试、lint、类型检查或构建命令。
- 总结剩余风险。
- 给出建议 commit message 或 PR 描述。

可选资源：

- `references/checklist.md`：提交前检查清单。
- `scripts/summarize-diff.ps1`：可选脚本，用于辅助汇总 diff。

## Skill 4: no-surprise-git

用途：约束 Git 操作，避免 Codex 做出用户没要求的危险动作。

适合触发：

- commit
- branch
- merge
- rebase
- reset
- stash
- checkout

建议行为：

- 不使用 `git reset --hard`，除非用户明确要求。
- 不使用 `git checkout -- <file>` 覆盖文件，除非用户明确要求。
- 不自动 force push。
- 不自动把所有文件 `git add .`，除非用户明确要求。
- 提交前先查看 diff，并按逻辑分组 stage。
- 如果发现无关改动，保留并说明。

可选资源：

- `references/git-safety.md`：安全 Git 操作准则。

## 推荐先创建的组合

第一批建议只创建三个：

- `safe-code-change`
- `chinese-collaboration`
- `preflight-check`

暂时不建议先创建 plugin。plugin 更适合下面这些情况：

- 有一组稳定脚本要长期复用。
- 有固定模板或资产要打包。
- 要给多个项目或团队成员分发。
- 需要 MCP、hooks、app connector 等能力。

## 后续创建位置

用户级 skills 可放在：

```text
C:\Users\94179\.codex\skills\
```

示例结构：

```text
C:\Users\94179\.codex\skills\safe-code-change\SKILL.md
C:\Users\94179\.codex\skills\chinese-collaboration\SKILL.md
C:\Users\94179\.codex\skills\preflight-check\SKILL.md
```

项目级 skills 可放在具体项目内：

```text
<project-root>\.agents\skills\
```

## 建议下一步

先把 `AGENTS.md` 放到一个真实项目里试用。如果使用中发现 Codex 反复出现某类问题，再把对应规则升级成 skill。
