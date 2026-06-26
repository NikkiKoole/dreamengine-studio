about tags and curating

I'd resist adding more tags just yet. I think you're at the point where you should start using the metadata instead of expanding it.

I'd tackle it in this order:

1. Add a status field

One field instead of many booleans.

status: active

Possible values:

active      // current, worth showing
showcase    // one of the best carts
retired     // replaced by something better
archive     // keep for history
hidden       // experiments, don't publish
2. Build a "Best of DreamEngine"

Pick 20–40 carts.

If someone lands on your GitHub Pages, these are the ones they should see first.

Everything else lives behind:

Browse all (427 carts)

instead of the other way around.

3. Add relationships

You already have things like inspiration and tags.

I'd add:

replaces: circles

or

successor: terrain2

Now retired carts automatically point people to the newer one.

4. Generate collections

Instead of one huge list, generate pages automatically.

For example:

🚀 Showcase
🎮 Games
🧪 Experiments
📚 Tutorials
🔊 Audio
🌍 Procedural
📐 Rendering
🗄 Archive

The metadata already gets you most of the way there.

5. Finally, retire aggressively

People don't care that you once made five circle tutorials.

Keep the best one.

Mark the rest:

status: retired
replaced_by: draw_primitives

They're still searchable, but they don't clutter the gallery.

After that...

The thing I'd build next is a small static site generator.

Not a CMS—just a script that reads index.hjson and produces:

a beautiful homepage
a "Best Of" page
category pages
search
retired carts hidden by default
"Related carts"
"This cart supersedes..."
"Built with DreamEngine version X"

At that point your cart collection stops feeling like a folder full of demos and starts feeling like a curated software library. I think that's the biggest leap in usability you can make now.
