// config for 05b-colorkey.c
// sprite 1 = yellow sun on a pink background (k = pink, Y = yellow, W = white)

module.exports = {
  sprites: {
    // slot 1 — sun with a pink background
    // pink pixels (k) vanish when you call colorkey(CLR_PINK)
    1: `
kkkkkkkkkkkkkkkk
kkkkkYYYYYYkkkk
kkkYYYYYYYYYYkk
kkYYYYWYYYYYYYk
kYYYYYYYYYYYYYk
kYYYYYYYYYYYYYk
kYYYYYYYYYYYYYk
kYYYYYYYYYYYYYk
kYYYYYYYYYYYYYk
kYYYYYYYYYYYYYk
kkYYYYYYYYYYYkk
kkkYYYYYYYYYkkk
kkkkkYYYYYkkkk
kkkkkkkkkkkkkkkk
kkkkkkkkkkkkkkkk
kkkkkkkkkkkkkkkk
`,
  },
}
