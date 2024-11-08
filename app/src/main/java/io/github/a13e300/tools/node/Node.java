package io.github.a13e300.tools.node;

import androidx.annotation.NonNull;

import java.util.List;

public class Node<T> {
    public int index;
    public T value;
    public List<Node<T>> children;

    public String getValueName() {
        return value == null ? "" : value.getClass().getName();
    }

    @NonNull
    @Override
    public String toString() {
        // json format
        return "{"
                + "\"index\":" + index
                + ",\"name\":\"" + getValueName() + "\""
                + ",\"value\":\"" + value + "\""
                + ",\"children\":" + children
                + "}";
    }
}
