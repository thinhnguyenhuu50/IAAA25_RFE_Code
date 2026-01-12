# UV Python Project Manager Guide

## Installation

### Linux/WSL2
```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

## Quick Start

### Install dependencies from pyproject.toml
```bash
uv sync
```

### Add dependencies
```bash
uv add requests numpy pandas
```

### Remove a dependency
```bash
uv remove package-name
```