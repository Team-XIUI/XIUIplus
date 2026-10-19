# Archived records

These audits, plans, implementation records and native investigations preserve evidence and earlier decisions. They may contain old source line numbers, ABI/build versions, completed work or task-specific authorization. They are not current implementation contracts or permission for new live experiments.

Use [module notes](../modules/README.md), [shared foundations](../foundations.md), [input ownership](../reference/input.md) and [development](../development.md) for current work. Consult a record only when its evidence matters to the task.

- [Feature-parity audit](feature-parity.md) and [implementation records](implementation/README.md); [combined historical view](feature-parity.html).
- [Code organization audit](2026-10-18-codebase-organization.md) and [Phoenix source audit](phoenix-source-audit.md).
- [Lua migration audit](lua-profile-migration-audit.md), [migration plan](lua-profile-migration-plan.md) and [SimpleLog proposal](simplelog-compatibility-plan.md).
- [Native mob/status evidence](native-mob-data-findings.md).
- [Controller research](controller-research-2026-10-14.md), [AgentLink evidence](controller-agentlink-2026-10-14.md) and [remaining-work snapshot](controller-plan-2026-10-18.md); [older controller diagram](controller-plan.html).
- [Earlier minimap validation](minimap-validation-2026-10-17.md).
- Pre-cleanup documentation snapshots: [shared foundations](foundations-2026-10-18.md), [module details](module-foundations-2026-10-18.md) and [input implementation](input-implementation-2026-10-18.md).

The combined feature HTML can be rebuilt explicitly with `python scripts/render_feature_parity.py`. Routine documentation updates regenerate the current settings inventory and check links; they do not refresh historical acceptance claims.
