# Plan Naming Convention Rule

Whenever creating or updating plan documents for this workspace:
1. Always store them inside the `.plan/` directory.
2. Strictly format the file name according to:
   ```text
   YYMMDD-[utc time]-[plan name].md
   ```
   - `YYMMDD`: 2-digit year, month, day (e.g., `261001`).
   - `[utc time]`: UTC time in `HHMM` (e.g., `1242`).
   - `[plan name]`: Descriptive name (e.g., `migration_plan`).

Example:
`.plan/261001-1242-migration_plan.md`
