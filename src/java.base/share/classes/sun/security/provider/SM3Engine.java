package sun.security.provider;

public interface SM3Engine extends Cloneable {

    void reset();

    void update(byte message);

    void update(byte[] message);

    void update(byte[] message, int offset, int length);

    void doFinal(byte[] out);

    void doFinal(byte[] out, int offset);

    byte[] doFinal();

    public SM3Engine clone() throws CloneNotSupportedException;
}
