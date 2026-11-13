// SPDX-License-Identifier: MIT
//
// Transparent overlay view that reports Qt Quick text fields to the Android autofill framework as virtual children.
// Qt Quick draws the real fields itself; this view only provides the structure, bounds and values, and forwards
// autofilled values back to C++. It neither takes focus nor consumes touches.
package io.github.deivadder.qtquickautofill;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.text.InputType;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;

public class AutofillOverlay extends View
{
    private static final String TAG = "QtQuickAutofill";

    // Same values as Autofill::Role in autofill.h
    private static final int ROLE_USERNAME = 1;
    private static final int ROLE_PASSWORD = 2;
    private static final int ROLE_NEW_PASSWORD = 3;
    private static final int ROLE_EMAIL = 4;
    private static final int ROLE_ONE_TIME_CODE = 5;

    private static final class Field
    {
        int role;
        // Physical pixels relative to the Qt window, which is laid out like this overlay
        final Rect windowRect = new Rect();
        String value = "";
    }

    private static AutofillOverlay s_overlay;

    private final SparseArray<Field> m_fields = new SparseArray<>();
    private int m_focusedId = 0;

    static native void nativeAutofilled(int id, String value);

    public AutofillOverlay(Context context)
    {
        super(context);
        setImportantForAutofill(IMPORTANT_FOR_AUTOFILL_YES);
        setClickable(false);
        setFocusable(false);
    }

    // --- Called from C++ (any thread); everything is posted to the UI thread ------------------------------------

    public static boolean install(Context context)
    {
        if (!(context instanceof Activity))
            return false;
        final Activity activity = (Activity) context;
        activity.runOnUiThread(() -> {
            if (s_overlay != null)
                return;
            s_overlay = new AutofillOverlay(activity);
            activity.addContentView(s_overlay, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                                                          ViewGroup.LayoutParams.MATCH_PARENT));
        });
        AutofillManager afm = activity.getSystemService(AutofillManager.class);
        return afm != null && afm.isEnabled();
    }

    public static void addField(int id, int role)
    {
        post(v -> {
            Field f = new Field();
            f.role = role;
            v.m_fields.put(id, f);
        });
    }

    public static void removeField(int id)
    {
        post(v -> {
            AutofillManager afm = v.manager();
            if (afm != null && v.m_focusedId == id)
                afm.notifyViewExited(v, id);
            if (v.m_focusedId == id)
                v.m_focusedId = 0;
            v.m_fields.remove(id);
        });
    }

    public static void focusChanged(int id, boolean focused, int x, int y, int w, int h, boolean requestFill)
    {
        post(v -> {
            Field f = v.m_fields.get(id);
            AutofillManager afm = v.manager();
            if (f == null || afm == null)
                return;
            f.windowRect.set(x, y, x + w, y + h);
            if (focused) {
                v.m_focusedId = id;
                afm.notifyViewEntered(v, id, v.toScreen(f.windowRect));
                // Gboard only answers inline-suggestion requests for the view that holds the input connection,
                // which is Qt's own view. An explicit request is answered by the autofill service directly.
                if (requestFill)
                    afm.requestAutofill(v, id, v.toScreen(f.windowRect));
            } else {
                if (v.m_focusedId == id)
                    v.m_focusedId = 0;
                afm.notifyViewExited(v, id);
            }
        });
    }

    public static void boundsChanged(int id, int x, int y, int w, int h)
    {
        post(v -> {
            Field f = v.m_fields.get(id);
            if (f != null)
                f.windowRect.set(x, y, x + w, y + h);
        });
    }

    public static void valueChanged(int id, String text)
    {
        post(v -> {
            Field f = v.m_fields.get(id);
            AutofillManager afm = v.manager();
            if (f == null)
                return;
            f.value = text;
            if (afm != null)
                afm.notifyValueChanged(v, id, AutofillValue.forText(text));
        });
    }

    public static void requestFill(int id, int x, int y, int w, int h)
    {
        post(v -> {
            Field f = v.m_fields.get(id);
            AutofillManager afm = v.manager();
            if (f == null || afm == null)
                return;
            f.windowRect.set(x, y, x + w, y + h);
            afm.requestAutofill(v, id, v.toScreen(f.windowRect));
        });
    }

    public static void commit()
    {
        post(v -> {
            AutofillManager afm = v.manager();
            if (afm != null)
                afm.commit();
        });
    }

    public static void cancel()
    {
        post(v -> {
            AutofillManager afm = v.manager();
            if (afm != null)
                afm.cancel();
        });
    }

    // --- Autofill framework -----------------------------------------------------------------------------------

    @Override
    public void onProvideAutofillVirtualStructure(ViewStructure structure, int flags)
    {
        structure.setClassName(getClass().getName());
        final int base = structure.addChildCount(m_fields.size());
        for (int i = 0; i < m_fields.size(); ++i) {
            final int id = m_fields.keyAt(i);
            final Field f = m_fields.valueAt(i);
            final ViewStructure child = structure.newChild(base + i);
            final boolean secret = f.role == ROLE_PASSWORD || f.role == ROLE_NEW_PASSWORD;
            child.setAutofillId(structure.getAutofillId(), id);
            child.setClassName("android.widget.EditText");
            child.setAutofillType(AUTOFILL_TYPE_TEXT);
            child.setAutofillHints(new String[] { hintFor(f.role) });
            child.setInputType(inputTypeFor(f.role));
            child.setAutofillValue(AutofillValue.forText(f.value));
            child.setDataIsSensitive(secret);
            child.setDimens(f.windowRect.left, f.windowRect.top, 0, 0, f.windowRect.width(),
                            f.windowRect.height());
            child.setVisibility(VISIBLE);
            child.setEnabled(true);
            child.setFocusable(true);
            child.setFocused(id == m_focusedId);
        }
    }

    @Override
    public void autofill(SparseArray<AutofillValue> values)
    {
        for (int i = 0; i < values.size(); ++i) {
            final int id = values.keyAt(i);
            final AutofillValue value = values.valueAt(i);
            final Field f = m_fields.get(id);
            if (f == null || value == null || !value.isText())
                continue;
            f.value = value.getTextValue().toString();
            nativeAutofilled(id, f.value);
        }
    }

    // --- Helpers ----------------------------------------------------------------------------------------------

    private interface OverlayAction
    {
        void run(AutofillOverlay overlay);
    }

    private static void post(OverlayAction action)
    {
        final AutofillOverlay v = s_overlay;
        if (v == null) {
            Log.w(TAG, "overlay not installed");
            return;
        }
        v.post(() -> action.run(v));
    }

    private Rect toScreen(Rect windowRect)
    {
        final int[] location = new int[2];
        getLocationOnScreen(location);
        final Rect r = new Rect(windowRect);
        r.offset(location[0], location[1]);
        return r;
    }

    private AutofillManager manager()
    {
        return getContext().getSystemService(AutofillManager.class);
    }

    private static String hintFor(int role)
    {
        switch (role) {
        case ROLE_PASSWORD: return AUTOFILL_HINT_PASSWORD;
        case ROLE_NEW_PASSWORD: return "newPassword";
        case ROLE_EMAIL: return AUTOFILL_HINT_EMAIL_ADDRESS;
        case ROLE_ONE_TIME_CODE: return "smsOTPCode";
        case ROLE_USERNAME:
        default: return AUTOFILL_HINT_USERNAME;
        }
    }

    private static int inputTypeFor(int role)
    {
        switch (role) {
        case ROLE_PASSWORD:
        case ROLE_NEW_PASSWORD: return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
        case ROLE_EMAIL: return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
        case ROLE_ONE_TIME_CODE: return InputType.TYPE_CLASS_NUMBER;
        default: return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL;
        }
    }
}
