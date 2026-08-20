package com.veyra.cam.service

import android.util.Base64
import android.util.Log
import java.security.MessageDigest
import java.security.SecureRandom

/**
 * C-2: out-of-band pairing state machine (server side = phone).
 *
 * Flow:
 *   IDLE -> (client connects) -> AWAITING_PIN (PIN shown on phone)
 *   AWAITING_PIN -> (client sends correct PIN + its X25519 pubkey) -> PAIRED (token issued)
 *   AWAITING_PIN -> (5 wrong PINs) -> LOCKED (30s cooldown)
 *
 * PIN comparison is constant-time (MessageDigest.isEqual). The pairing token is
 * a 32-byte CSPRNG value required on every subsequent control command.
 */
class PairingManager {
    enum class State { IDLE, AWAITING_PIN, PAIRED, LOCKED }

    companion object {
        private const val TAG = "PairingManager"
        private const val MAX_ATTEMPTS = 5
        private const val LOCKOUT_MS = 30_000L
        private const val PIN_LENGTH = 6
    }

    @Volatile private var state = State.IDLE
    @Volatile private var currentPin: String? = null
    @Volatile private var authToken: String? = null
    @Volatile private var failedAttempts = 0
    @Volatile private var lockUntilMs = 0L
    @Volatile private var pairedClientFingerprint: String? = null

    private val secureRandom = SecureRandom()

    @Synchronized
    fun startPairing(clientFingerprint: String): String? {
        if (state == State.LOCKED && System.currentTimeMillis() < lockUntilMs) {
            return null
        }
        state = State.AWAITING_PIN
        currentPin = generatePin()
        authToken = null
        pairedClientFingerprint = clientFingerprint
        return currentPin
    }

    @Synchronized
    fun verifyPin(input: String): PairingResult {
        if (state == State.LOCKED) {
            if (System.currentTimeMillis() < lockUntilMs) {
                return PairingResult.LOCKED
            }
            state = State.AWAITING_PIN
            failedAttempts = 0
        }
        if (state != State.AWAITING_PIN) {
            return PairingResult.NOT_AWAITING
        }

        val expected = currentPin
        if (expected == null) {
            return PairingResult.NOT_AWAITING
        }

        val matches = MessageDigest.isEqual(
            input.toByteArray(Charsets.US_ASCII),
            expected.toByteArray(Charsets.US_ASCII)
        )
        if (!matches) {
            failedAttempts++
            if (failedAttempts >= MAX_ATTEMPTS) {
                state = State.LOCKED
                lockUntilMs = System.currentTimeMillis() + LOCKOUT_MS
                Log.w(TAG, "Pairing locked out after $MAX_ATTEMPTS failed attempts")
            }
            return PairingResult.INVALID_PIN
        }

        // Success: mint a fresh token and move to PAIRED.
        val tokenBytes = ByteArray(32)
        secureRandom.nextBytes(tokenBytes)
        authToken = Base64.encodeToString(tokenBytes, Base64.NO_WRAP)
        state = State.PAIRED
        failedAttempts = 0
        Log.i(TAG, "Pairing complete")
        return PairingResult.OK
    }

    @Synchronized
    fun isTokenValid(candidate: String?): Boolean {
        val expected = authToken ?: return false
        return candidate != null &&
            MessageDigest.isEqual(
                candidate.toByteArray(Charsets.US_ASCII),
                expected.toByteArray(Charsets.US_ASCII)
            )
    }

    @Synchronized
    fun reset() {
        state = State.IDLE
        currentPin = null
        authToken = null
        failedAttempts = 0
        lockUntilMs = 0L
        pairedClientFingerprint = null
    }

    @Synchronized
    fun isPaired(): Boolean = state == State.PAIRED

    @Synchronized
    fun getCurrentPin(): String? = if (state == State.AWAITING_PIN) currentPin else null

    @Synchronized
    fun getAuthToken(): String? = authToken

    private fun generatePin(): String {
        val sb = StringBuilder(PIN_LENGTH)
        for (i in 0 until PIN_LENGTH) {
            sb.append(secureRandom.nextInt(10))
        }
        return sb.toString()
    }

    enum class PairingResult { OK, INVALID_PIN, LOCKED, NOT_AWAITING }
}