import { z } from 'zod';

export const FavoriteSchema = z.object({
  // songId: z.string().length(8), // Enforce that 8-char string length
  href: z.string(),
  mtime: z.number().int().nonnegative(), // Unix timestamp in seconds
});

export const FavoritesSchema = z.array(FavoriteSchema);

export const SettingsSchema = z.object({
  // Explicitly defined "Global" settings
  showPlayerSettings: z.boolean().default(true),
  theme: z.string().max(20).default('msdos'),
  tempo: z.number().min(0.1).max(2).default(1),
}).catchall(
  z.union([z.string(), z.number(), z.boolean()])
);
