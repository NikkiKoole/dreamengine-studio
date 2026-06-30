# AI-Friendly Frontmatter

STATUS: SHIPPED — adopted convention for AI-friendly frontmatter (the de:meta / journal-header house style).

## Purpose

As the DreamEngine knowledge base grows, documents should become easier for both humans and AI agents to understand.

Rather than introducing a new file format, DreamEngine adopts a small, standard YAML frontmatter block at the beginning of Markdown documents.

This keeps every document valid Markdown while providing lightweight structured metadata.

## Principles

* Keep the metadata minimal.
* Prefer existing conventions over inventing new ones.
* Metadata should help discovery, not become maintenance.
* If information can be inferred (Git history, folder location, filename), do not duplicate it.

## Initial Frontmatter

```yaml
---
title: Roads as a Convergence Layer

summary: >
  Roads evolved into the convergence layer between
  terrain, traffic and rendering.

concepts:
  - roads
  - terrain
  - rendering

status: accepted
---
```

## Fields

### `title`

Human-readable document title.

### `summary`

A short one or two sentence description of the document.

This allows AI tooling to quickly determine whether the document is relevant before loading the full contents.

### `concepts`

A small list of domain concepts discussed by the document.

Unlike free-form tags, concepts should represent stable parts of the DreamEngine domain.

Examples:

* roads
* terrain
* streaming
* renderer
* traffic
* carts
* simulation

### `status`

Current lifecycle state of the document.

Initial values:

* draft
* observed
* review
* accepted
* superseded
* archived

## Adoption

This is an opt-in convention.

Existing documents do not need to be updated immediately.

New documents should use the frontmatter, and older documents can be updated gradually when they are edited.

## Future

This metadata provides a foundation for future tooling such as semantic search, AI context assembly, documentation generation and knowledge indexing (for example QMD), while remaining fully compatible with standard Markdown tooling.
