question: how would i make a system that would be able to dstill all the md files and get nfo out them, its too much for you or another ganet, are there solutions (not our field reseacrh but the totality) you mentioned obsidian a while ago, are there other things or olutions to this ?
Thought for 7s

Yes. You’re basically describing RAG + knowledge graph + generated wiki, but I would not start with a big “AI knowledge platform.” For DreamEngine I’d build it in your own style: small tools, local files, derived outputs.

The best architecture is probably:

Markdown docs / carts / ADRs / field-notes
        ↓
ingest + parse frontmatter + links
        ↓
chunks + summaries + extracted entities
        ↓
search index + relationship graph
        ↓
build-context / build-compendium / ask-docs
The practical stack I’d choose

For your case, I’d start with Obsidian + a custom Node script + a vector index.

Obsidian is good as the human-facing layer: backlinks, graph view, local Markdown, fast editing. But Obsidian alone won’t solve agent context. You need a tool that reads the repo and builds machine-friendly outputs.

A minimal first tool:

node tools/knowledge-index.js

It would produce:

.build/knowledge/
  docs.json
  chunks.json
  links.json
  graph.json
  summaries.md
  domains/roads.md
  domains/audio.md

Then later:

node tools/build-context.js roads
node tools/build-context.js cart:coaster
node tools/build-context.js task:retire-carts

That gives agents a clean briefing instead of dumping the whole repo into context.

Existing solutions worth knowing

LlamaIndex is probably the most relevant framework if you want to build this yourself. It supports document ingestion, retrieval, agents, and knowledge graph indexes; their docs describe automatic graph construction from unstructured text and graph-based querying.

Dify and RAGFlow are more complete “platforms” for RAG workflows. They are useful if you want a web UI, document ingestion, and agent-like workflows without building everything yourself. A recent overview describes Dify as a visual workflow/RAG platform and RAGFlow as strong on document understanding and GraphRAG-style retrieval.

Docusaurus + DocSearch / AskAI is interesting if you want the docs to become a public-facing site. Docusaurus 3.9 added DocSearch v4 support with AskAI, which can answer questions from documentation content.

WeKnora is worth watching because it explicitly aims at turning raw documents into a self-maintaining interlinked Markdown knowledge base with an interactive knowledge graph. That sounds very close to your “distill all MD files” problem.

There’s also the emerging LLM Wiki pattern: instead of only doing RAG over raw documents, an agent maintains a structured Markdown wiki as an intermediate layer. That idea fits DreamEngine very well: raw docs → distilled wiki → context packs.

My recommendation

Don’t start with a full platform.

Start with this:

1. Keep Markdown as source of truth.
2. Add light frontmatter to important docs.
3. Build a local indexer.
4. Generate summaries per domain.
5. Add build-context.
6. Later add embeddings / LlamaIndex / graph search.

The key idea:

Don’t ask agents to read everything. Build a system that tells them what matters.

That’s the DreamEngine-shaped solution.
