# Future Research Directions

> This document captures areas of research that appear important for the future evolution of DreamEngine.
>
> Unlike field notes, these are not observations.
> They are promising directions for future exploration.
>
> They should be treated as questions rather than plans.

---

# Introduction

The first phase of DreamEngine focused primarily on building capabilities.

As the project evolved, a second theme emerged: improving the way capabilities are created.

This suggests that DreamEngine's future growth may depend less on adding individual features and more on improving the systems that support exploration, collaboration and learning.

The following areas appear particularly promising.

---

# 1. Agent Orchestration

DreamEngine already benefits from multiple specialized agents working on different tasks.

The next challenge is understanding how those agents should collaborate.

Questions include:

* How should work be divided?
* Which agent owns which decisions?
* How are reviews performed?
* When should an agent ask another agent for help?
* Which work should remain human?

This is less a programming problem than an organizational one.

---

# 2. Context Assembly

As documentation grows, manually selecting context becomes increasingly inefficient.

Future agents should receive task-specific context automatically.

Rather than asking an agent to discover relevant knowledge, DreamEngine should assemble:

* design documents
* ADRs
* related carts
* APIs
* field notes
* verification steps
* previous experiments

The objective is not larger context windows, but better context.

---

# 3. Knowledge Graphs

DreamEngine increasingly contains relationships that are difficult to represent through folders alone.

Examples include:

* carts using shared APIs
* APIs promoted from experiments
* field notes referencing design documents
* tools generating project artifacts

Representing these relationships as a graph could improve both navigation and context assembly.

---

# 4. Promotion Systems

Promotion currently exists as a philosophy.

Future work could make promotion an explicit subsystem.

Questions include:

* When has an idea earned promotion?
* Can promotion candidates be detected automatically?
* Which repeated patterns deserve engine support?
* Which ideas should remain experiments?

Promotion may eventually become one of DreamEngine's core mechanisms.

---

# 5. Knowledge Preservation

Field notes introduced the idea that understanding itself should be preserved.

Future work may explore:

* research journals
* syntheses
* glossaries
* generated compendiums
* historical evolution of ideas

The objective is to preserve not only software, but also the reasoning that shaped it.

---

# 6. Human–AI Collaboration

One of the most important questions remains intentionally unresolved.

What should remain fundamentally human?

Current candidates include:

* curiosity
* taste
* long-term direction
* changing the North Star
* deciding what is worth exploring

Automation should expand human creativity rather than replace it.

---

# Areas Worth Studying

Several broader disciplines appear increasingly relevant.

These include:

* knowledge architecture
* software evolution
* organizational design
* systems thinking
* human-computer collaboration
* information retrieval
* knowledge graphs

These fields may ultimately influence DreamEngine more than graphics or rendering techniques.

---

# A Guiding Question

Throughout this research one question repeatedly emerged.

Instead of asking:

> "How can AI build more software?"

DreamEngine increasingly asks:

> "How can software, tools and AI help one curious person explore more ideas?"

That distinction may prove to be one of the project's defining characteristics.

---

# Closing Thoughts

DreamEngine does not appear to be converging toward a finished engine.

Instead, it appears to be evolving toward a personal environment for exploration, experimentation and learning.

The engine, tools, documentation and workflows are all expressions of that larger process.

If this trajectory continues, future work will likely focus less on adding features and more on improving the way new understanding is created, captured and reused.

The project becomes not only a place where software is built, but a place where better ways of building software are continuously discovered.
