# Stage 2 common coast art templates

The PNG is ordered left-to-right, top-to-bottom. Each panel is one
32px native tile enlarged 6× with nearest-neighbour sampling.

Art constraints:

- Keep the outermost two native pixels unchanged; they are the seam contract.
- Keep transparent water transparent.
- Preserve land/water polarity and the visible crossing on every edge.
- Use the `stage1_islet.png` palette and pixel density as the style reference.
- No resampling, antialiasing, gradients, text, or arbitrary vertical flips.
- Interior details may cross a tile only through a separately authored metatile.

| Panel | Uses | Mask | North | East | South | West |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 16 | `1100` | 2 | 2 | 2 | 2 |
| 2 | 14 | `0011` | 0 | 2 | 0 | 2 |
| 3 | 12 | `0011` | 2 | 1 | 2 | 1 |
| 4 | 9 | `0011` | 1 | 1 | 1 | 1 |
| 5 | 8 | `0100` | 2 | 2 | 2 | 2 |
| 6 | 8 | `0100` | 1 | 1 | 1 | 1 |
| 7 | 7 | `1000` | 2 | 2 | 2 | 2 |
| 8 | 7 | `0011` | 0 | 1 | 0 | 1 |
| 9 | 7 | `0011` | 1 | 2 | 1 | 2 |
| 10 | 6 | `0100` | 2 | 1 | 2 | 1 |
| 11 | 6 | `1000` | 1 | 1 | 1 | 1 |
| 12 | 6 | `1011` | 1 | 2 | 1 | 2 |

The generated atlas remains the fallback. Only validated painted variants
should replace or overlay these signatures at runtime.
