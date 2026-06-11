# project-name

> Replace this line with a short description of your project.

![CI](https://github.com/YOUR_USERNAME/YOUR_REPO/actions/workflows/ci.yml/badge.svg)

---

## Using this template

This is a GitHub template repo. To start a new project:

1. Click **"Use this template"** → **"Create a new repository"** on GitHub
2. Clone your new repo
3. Run `uv init .` to initialise uv in the existing folder
4. Run `bash setup-pipeline.sh` to activate local git hooks
5. Update `pyproject.toml` — set `name`, `description`, and `requires-python`
6. Replace the CI badge URL above with your repo's URL

---

## Prerequisites

Install these globally with pipx (one-time, not per project):

```bash
pipx install pre-commit
pipx install commitizen
pipx install detect-secrets
```

---

## Pipeline overview

### Local hooks (run on every `git commit`)

| Hook | What it does |
|---|---|
| detect-secrets | Blocks commits containing credentials or secrets |
| ruff | Lints and auto-fixes Python code |
| ruff-format | Formats Python code |
| clang-format | Checks C/C++ formatting |
| trailing-whitespace | Cleans up trailing whitespace |
| commitizen | Enforces Conventional Commits message format |

### GitHub Actions CI (runs on every push and PR)

| Job | What it does |
|---|---|
| Lint | ruff + clang-format check |
| Type Check | mypy |
| Security | detect-secrets + pip-audit |
| Tests | pytest |

---

## Commit message format

This repo uses [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

Types: feat, fix, docs, style, refactor, test, chore, ci
```

Examples:
```
feat(parser): add support for nested structs
fix(auth): handle null token on refresh
chore(deps): bump ruff to v0.5.0
```

---

## Adding a new language

1. Find the pre-commit hook for the language at [pre-commit.com/hooks](https://pre-commit.com/hooks.html)
2. Add a block to `.pre-commit-config.yaml`
3. Add a step to `.github/workflows/ci.yml`
