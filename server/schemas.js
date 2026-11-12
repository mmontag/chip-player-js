import { z } from 'zod';

export const FavoriteSchema = z.object({
  // songId: z.string().length(8), // Enforce that 8-char string length
  path: z.string(),
  subtune: z.number().int().nonnegative().default(0), // 0-based sub-tune index
  mtime: z.number().int().nonnegative(), // Unix timestamp in seconds
});

export const FavoritesSchema = z.array(FavoriteSchema);

export const SettingsSchema = z.object({
  // Explicitly defined "Global" settings
  showPlayerSettings: z.boolean().default(true),
  showVisualizer: z.boolean().default(false),
  theme: z.string().max(20).default('msdos'),
  tempo: z.number().min(0.1).max(2).default(1),
  silenceDuration: z.number().min(-1).max(60).default(-1),
  showLoopArea: z.boolean().default(true),
}).catchall(
  z.union([z.string(), z.number(), z.boolean()])
);

export const PlaybackSchema = z.object({
  songId: z.string().length(8),
  userId: z.string().optional(),
  subtune: z.number().int().nonnegative().default(0), // 0-based sub-tune index
  durationMs: z.number().int().nonnegative().default(0),
});
