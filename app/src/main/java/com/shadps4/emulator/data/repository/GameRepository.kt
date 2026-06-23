package com.shadps4.emulator.data.repository

import com.shadps4.emulator.data.model.GameInfo
import com.shadps4.emulator.data.model.SampleGames
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Repository of installed games. In this prototype it just exposes the sample
 * data; the real implementation will scan a user-selected folder and persist
 * discovered titles to a local database (Room) in a future step.
 */
class GameRepository {

    private val _games = MutableStateFlow<List<GameInfo>>(SampleGames.all)
    val games: StateFlow<List<GameInfo>> = _games.asStateFlow()

    fun getGameById(id: String): GameInfo? = _games.value.firstOrNull { it.id == id }

    fun search(query: String): List<GameInfo> {
        val q = query.trim().lowercase()
        if (q.isEmpty()) return _games.value
        return _games.value.filter {
            it.title.lowercase().contains(q) || it.serial.lowercase().contains(q)
        }
    }

    fun addGame(game: GameInfo) {
        _games.value = (_games.value + game).distinctBy { it.id }
    }

    fun removeGame(id: String) {
        _games.value = _games.value.filterNot { it.id == id }
    }

    fun recent(): List<GameInfo> = _games.value
        .filter { it.lastPlayedMs > 0L }
        .sortedByDescending { it.lastPlayedMs }
}
