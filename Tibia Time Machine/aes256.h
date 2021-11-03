#pragma once

#define BLOCK_SIZE 16

class Aes256 {

public:
	Aes256(const unsigned char *key);
	~Aes256();

	unsigned long encrypt_start(const unsigned long plain_length);
	unsigned long encrypt_continue(const unsigned char* plain, const unsigned long plain_length, unsigned char *encrypted);
	unsigned long encrypt_end(unsigned char *encrypted);

	unsigned long decrypt_start(const unsigned long encrypted_length);
	unsigned long decrypt_continue(const unsigned char* encrypted, const unsigned long encrypted_length, unsigned char *plain);
	unsigned long decrypt_end(unsigned char *plain);

	static unsigned long encrypt(const unsigned char *key, const unsigned char* plain, const unsigned long plain_length, unsigned char *encrypted);
	static unsigned long encrypt(const unsigned char *key, unsigned char* plain, unsigned long plain_length);
	static void encrypt(const unsigned char *key, unsigned char *buffer);

	static unsigned long decrypt(const unsigned char *key, const unsigned char* encrypted, const unsigned long encrypted_length, unsigned char *plain);
	static unsigned long decrypt(const unsigned char *key, unsigned char* encrypted, unsigned long encrypted_length);
	static void decrypt(const unsigned char *key, unsigned char *buffer);

private:
	const unsigned char *m_key;
	unsigned char        m_buffer[3 * BLOCK_SIZE];
	unsigned char        m_buffer_pos;
	unsigned long        m_i;

	static void expand_enc_key(unsigned char *rkey, unsigned char *rc);
	static void expand_dec_key(unsigned char *rkey, unsigned char *rc);

	static void sub_bytes(unsigned char *buffer);
	static void sub_bytes_inv(unsigned char *buffer);

	static void copy_key(const unsigned char *key, unsigned char* rkey);

	static void add_round_key(unsigned char *rkey, unsigned char *buffer, const unsigned char round);

	static void shift_rows(unsigned char *buffer);
	static void shift_rows_inv(unsigned char *buffer);

	static void mix_columns(unsigned char *buffer);
	static void mix_columns_inv(unsigned char *buffer);
};
