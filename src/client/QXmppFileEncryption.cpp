// SPDX-FileCopyrightText: 2022 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppFileEncryption.h"

#include "Crypto.h"

#include <QByteArray>

#undef min

using namespace QXmpp::Private::Crypto;

constexpr std::size_t AES128_BLOCK_SIZE = 128 / 8;
constexpr std::size_t AES256_BLOCK_SIZE = 256 / 8;
constexpr int GCM_IV_SIZE = 12;

namespace QXmpp::Private::Encryption {

static std::size_t blockSize(QXmpp::Cipher cipher)
{
    switch (cipher) {
    case Aes128GcmNoPad:
        return AES128_BLOCK_SIZE;
    case Aes256GcmNoPad:
    case Aes256CbcPkcs7:
        return AES256_BLOCK_SIZE;
    }
    Q_UNREACHABLE();
}

static std::size_t roundUpToBlockSize(qint64 size, std::size_t blockSize)
{
    Q_ASSERT(size >= 0);
    return (size / blockSize + 1) * blockSize;
}

QByteArray process(const QByteArray &data, QXmpp::Cipher cipherConfig, Direction direction, const QByteArray &key, const QByteArray &iv)
{
    switch (cipherConfig) {
    case Aes128GcmNoPad:
    case Aes256GcmNoPad:
        return direction == Encode
            ? aesGcmEncrypt(data, key, iv)
            : aesGcmDecrypt(data, key, iv);
    case Aes256CbcPkcs7:
        return direction == Encode
            ? aesCbcEncrypt(data, key, iv)
            : aesCbcDecrypt(data, key, iv);
    }
    Q_UNREACHABLE();
}

QByteArray generateKey(QXmpp::Cipher cipher)
{
    return randomBytes(int(blockSize(cipher)));
}

QByteArray generateInitializationVector(QXmpp::Cipher config)
{
    switch (config) {
    case Aes128GcmNoPad:
    case Aes256GcmNoPad:
        return randomBytes(GCM_IV_SIZE);
    case Aes256CbcPkcs7:
        return randomBytes(int(blockSize(config)));
    }
    Q_UNREACHABLE();
}

EncryptionDevice::EncryptionDevice(std::unique_ptr<QIODevice> input,
                                   Cipher config,
                                   const QByteArray &key,
                                   const QByteArray &iv)
    : m_cipherConfig(config),
      m_input(std::move(input)),
      m_cipher(std::make_unique<CipherContext>(
          config,
          Crypto::Encrypt,
          key,
          iv))
{
    // output must not be sequential
    Q_ASSERT(!m_input->isSequential());

    Q_ASSERT(m_outputBuffer.empty());

    setOpenMode(m_input->openMode() & QIODevice::ReadOnly);

    Q_ASSERT(m_cipher->validKeyLength(int(key.length())));
}

EncryptionDevice::~EncryptionDevice() = default;

bool EncryptionDevice::open(OpenMode mode)
{
    return m_input->open(mode);
}

void EncryptionDevice::close()
{
    m_input->close();
}

bool EncryptionDevice::isSequential() const
{
    return false;
}

qint64 EncryptionDevice::size() const
{
    switch (m_cipherConfig) {
    case Aes128GcmNoPad:
    case Aes256GcmNoPad:
        return m_input->size();
    case Aes256CbcPkcs7: {
        // padding is done with 128 bits blocks
        return roundUpToBlockSize(m_input->size(), 128 / 8);
    }
    }
    Q_UNREACHABLE();
}

qint64 EncryptionDevice::readData(char *data, qint64 len)
{
    auto requestedLen = len;
    qint64 read = 0;

    {
        // try to read from output buffer
        qint64 outputBufferRead = std::min(qint64(m_outputBuffer.size()), len);
        std::copy_n(m_outputBuffer.cbegin(), outputBufferRead, data);
        m_outputBuffer.erase(m_outputBuffer.begin(), m_outputBuffer.begin() + outputBufferRead);
        read += outputBufferRead;
        len -= outputBufferRead;
    }

    if (len > 0) {
        // read from input and encrypt new data

        // output buffer is empty here
        Q_ASSERT(m_outputBuffer.empty());

        // read unencrypted data (may read one block more than needed)
        auto inputBufferSize = roundUpToBlockSize(len, blockSize(m_cipherConfig));
        Q_ASSERT(inputBufferSize > 0);
        QByteArray inputBuffer;
        inputBuffer.resize(inputBufferSize);
        inputBuffer.resize(m_input->read(inputBuffer.data(), inputBufferSize));

        // process input buffer
        auto processed = m_cipher->update(inputBuffer);
        if (m_input->atEnd()) {
            m_finalized = true;
            processed += m_cipher->finalize();
        }

        // split up into part for user and put rest into output buffer
        auto processedReadBytes = std::min(qint64(processed.size()), len);
        std::copy_n(processed.constData(), processedReadBytes, data + read);
        read += processedReadBytes;
        len -= processedReadBytes;

        Q_ASSERT(processed.size() >= processedReadBytes);
        auto restBytes = size_t(processed.size() - processedReadBytes);
        m_outputBuffer.resize(restBytes);
        std::copy_n(processed.constData() + processedReadBytes, restBytes, m_outputBuffer.data());
    }

    Q_ASSERT((len + read) == requestedLen);
    return read;
}

qint64 EncryptionDevice::writeData(const char *, qint64)
{
    return 0;
}

bool EncryptionDevice::atEnd() const
{
    return m_finalized && m_outputBuffer.empty();
}

DecryptionDevice::DecryptionDevice(std::unique_ptr<QIODevice> input,
                                   Cipher config,
                                   const QByteArray &key,
                                   const QByteArray &iv)
    : m_cipherConfig(config),
      m_output(std::move(input)),
      m_cipher(std::make_unique<CipherContext>(
          config,
          Crypto::Decrypt,
          key,
          iv))
{
    // output must not be sequential
    Q_ASSERT(!m_output->isSequential());

    Q_ASSERT(m_outputBuffer.empty());

    setOpenMode(m_output->openMode() & QIODevice::WriteOnly);

    Q_ASSERT(m_cipher->validKeyLength(int(key.length())));
}

DecryptionDevice::~DecryptionDevice() = default;

bool DecryptionDevice::open(OpenMode mode)
{
    return m_output->open(mode);
}

void DecryptionDevice::close()
{
    finish();
    m_output->close();
}

bool DecryptionDevice::isSequential() const
{
    return true;
}

qint64 DecryptionDevice::size() const
{
    return 0;
}

qint64 DecryptionDevice::readData(char *, qint64)
{
    return 0;
}

qint64 DecryptionDevice::writeData(const char *data, qint64 len)
{
    auto decrypted = m_cipher->update(QByteArray(data, len));
    m_output->write(decrypted.constData(), decrypted.size());
    return len;
}

void DecryptionDevice::finish()
{
    auto finalized = m_cipher->finalize();
    if (!finalized.isEmpty()) {
        m_output->write(finalized.constData(), finalized.size());
    }
}

}  // namespace QXmpp::Private::Encryption
