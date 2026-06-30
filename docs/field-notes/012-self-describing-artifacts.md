---

title: Self-Describing Artifacts

summary: >
DreamEngine should strive for artifacts that describe themselves. Metadata,
documentation and tooling should live alongside the artifact itself, allowing
humans and machines to understand the repository without maintaining duplicate
sources of truth.

concepts:

* documentation
* introspection
* workflow
* tooling
* repository

## status: review

# Self-Describing Artifacts

## Observation

Several independent design discussions converged on the same underlying idea.

Originally these appeared to be unrelated topics:

* YAML frontmatter
* tool headers
* cart manifests
* generated documentation
* AI context assembly
* QMD indexing

The common principle is not AI.

The common principle is **self-description**.

## Principle

Every artifact in the repository should contain enough information to explain itself.

Examples:

* A document explains what it is about.
* A tool explains when it should be used.
* A cart explains its purpose.
* An API explains what it exposes.
* A field note explains where it sits in the knowledge lifecycle.

The artifact becomes the primary source of truth.

Everything else can be generated.

## Why

A self-describing repository benefits both humans and machines.

Humans can discover and understand artifacts more easily.

AI agents can retrieve relevant information without relying on hidden prompts or project-specific knowledge.

Search, documentation and indexes become products of the repository rather than manually maintained assets.

## One Source of Truth

DreamEngine should avoid maintaining duplicate information.

Examples:

Instead of:

* a tool
* a separate tool registry

prefer:

* a tool with a descriptive header
* a generated tool index

Instead of:

* Markdown
* an external metadata database

prefer:

* Markdown with lightweight frontmatter
* generated indexes and search databases

Generated artifacts should always derive from the primary source rather than becoming sources themselves.

## Repository Introspection

The long-term goal is a repository capable of answering questions about itself.

Examples:

* What tools exist?
* Which cart should I use?
* Which documents discuss roads?
* Which field notes are accepted?
* Which APIs expose terrain functionality?

These answers should come from the repository itself rather than external documentation.

## Relationship to AI

AI is not the motivation.

AI is simply another consumer of a well-structured repository.

The same metadata should improve:

* human navigation
* generated documentation
* search
* repository tooling
* AI context assembly

No metadata should exist solely for one AI model.

## Inspiration

This philosophy draws inspiration from several longstanding software ideas:

* Smalltalk's introspection.
* Literate programming.
* Unix programs that describe themselves.
* Self-documenting APIs.

DreamEngine extends these ideas beyond source code to the repository as a whole.

## Summary

The objective is not to build an AI-first repository.

The objective is to build a repository that explains itself.

If every artifact can answer:

* What am I?
* Why do I exist?
* How should I be used?

then documentation, indexes, search systems and AI tooling become natural views over the repository rather than separate systems that must be maintained.
