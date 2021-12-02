#include "Map.h"

namespace Terrain
{
    void Map::GetChunkPositionFromChunkId(u16 chunkId, u16& x, u16& y) const
    {
        x = chunkId % MAP_CHUNKS_PER_MAP_STRIDE;
        y = chunkId / MAP_CHUNKS_PER_MAP_STRIDE;
    }

    vec2 Map::WorldPositionToADTCoordinates(const vec3& position)
    {
        // This is translated to remap positions [-17066 .. 17066] to [0 ..  34132]
        // This is because we want the Chunk Pos to be between [0 .. 64] and not [-32 .. 32]

        // We have to flip "X" and "Y" here due to 3D -> 2D
        return vec2(Terrain::MAP_HALF_SIZE - position.y, Terrain::MAP_HALF_SIZE - position.x);
    }
    vec2 Map::GetChunkFromAdtPosition(const vec2& adtPosition)
    {
        return adtPosition / Terrain::MAP_CHUNK_SIZE;
    }
    u32 Map::GetChunkIDFromChunkPos(const vec2& chunkPos)
    {
        return Math::FloorToInt(chunkPos.x) + (Math::FloorToInt(chunkPos.y) * Terrain::MAP_CHUNKS_PER_MAP_STRIDE);
    }
}