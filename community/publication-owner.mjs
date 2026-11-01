export function canManagePublication(entry, ownerHash, discordId) {
  return !entry || (entry.owner === ownerHash && !!entry.discordId && entry.discordId === discordId);
}
