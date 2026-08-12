# notes: the scratchpad

Free-form writing that isn't code and isn't (yet) a doc: marketing copy drafts, store
blurbs, post text, a paragraph you're chewing on, a list you'll act on tomorrow.

**Write it in the editor.** Docs tab, then the **notes** group in the sidebar, then
`+ new note`; or open any note and hit **✎ edit**. It saves straight to this folder.
Notes autosave a beat after you stop typing. Every other doc in `docs/` needs an
explicit ⌘S, so a real doc can't drift under you while you read it.

Everything here is git-tracked, so a draft started on one machine is there on the next
one, and the Docs-tab search finds it like any other doc.

## What this folder is NOT

The doc linters skip `notes/` on purpose (the same carve-out `archive/` gets): no
`STATUS` line, no cross-reference rules, no freshness nagging. That's what makes it safe
to write badly in here. The flip side is that nothing will ever tell you a note rotted.

So when a note stops being a draft, **move it to its real home** and delete it here:

| it turned into… | home |
|---|---|
| go-to-market copy for a product | `docs/marketing/<product>/` |
| store listing text | `apps/<name>/app.json` + `apps/<name>/press.md` |
| a design idea worth keeping | `docs/design/<topic>.md` (plus a `STATUS` line) |
| a settled "we did/didn't do X" | an ADR in `docs/decisions/` |
| something that changed our understanding | a note in `docs/field-notes/` |

A note that never graduates is fine. A note that quietly became the only copy of
something important is the failure mode.
