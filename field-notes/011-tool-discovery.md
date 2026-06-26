---

title: Tool Discovery and Documentation

summary: >
Defines a lightweight convention for self-documenting repository tools.
Tool metadata lives inside the tool itself and can later be used to generate
human and AI-friendly documentation.

concepts:

* tools
* workflow
* documentation
* agents

## status: observed

# Tool Discovery and Documentation

## Motivation

DreamEngine contains an increasing number of local tools.

These tools support building carts, validating documentation, generating indexes, checking repository health and performing many other maintenance tasks.

As the collection grows, two questions become increasingly common:

* Which tool should I use?
* When should I use it?

This applies equally to humans and AI agents.

## Principle

Every tool should describe itself.

Instead of maintaining a separate registry or manifest, each tool should contain a small metadata header.

The tool becomes the single source of truth.

Everything else can be generated from those headers.

This avoids duplicated information and keeps documentation close to the implementation.

## Proposed Header

Each tool should contain a small comment block near the top of the file.

Example:

```javascript
#!/usr/bin/env node

/**
 * @tool cart-info
 * @summary Orient yourself on a single cart.
 * @category carts
 * @when Before modifying or investigating a cart.
 * @safe read-only
 *
 * Usage:
 *   node tools/cart-info.js <cart-name>
 */
```

## Header Fields

### `@tool`

Stable tool name.

### `@summary`

One sentence describing the purpose of the tool.

### `@category`

Broad grouping.

Suggested categories:

* carts
* build
* docs
* site
* lint
* audio
* analysis
* maintenance

### `@when`

Describe when the tool should be used.

This is often more valuable than a technical description.

### `@safe`

Expected safety level.

Suggested values:

* read-only
* modifies-docs
* modifies-generated-files
* modifies-source
* destructive

## Generated Documentation

A future tool should scan every script in `tools/` and generate a repository tool index.

For example:

```text
docs/tools.md
```

This generated documentation should provide:

* available tools
* purpose
* usage
* safety level
* when to use the tool

The generated index becomes the primary reference for both humans and AI agents.

## Future Integration

This convention intentionally remains lightweight.

Later tooling may use the same headers to:

* generate CLI help
* build searchable tool indexes
* expose tools through MCP
* provide agent startup guidance
* validate undocumented tools

The metadata already exists inside the tool, so no additional manifests are required.

## Adoption

This convention is incremental.

Existing tools do not need to be updated immediately.

Whenever a tool is modified, its header can be added or improved.

New tools should include the header from the start.
